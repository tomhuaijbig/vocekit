using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Speech.AudioFormat;
using System.Speech.Recognition;
using System.Text;
using System.Threading;
using System.Web.Script.Serialization;

namespace VoceKit.WindowsSpeech
{
    internal sealed class Options
    {
        internal string Command;
        internal string Mode;
        internal string RunId;
        internal string Language = "follow-windows";
        internal int SampleRate = 16000;
        internal int Channels = 1;
        internal int Bits = 16;
    }

    internal sealed class ArgumentExceptionWithCode : Exception
    {
        internal ArgumentExceptionWithCode(string message)
            : base(message)
        {
        }
    }

    internal sealed class EventWriter
    {
        private readonly object gate = new object();
        private readonly TextWriter output;
        private readonly JavaScriptSerializer serializer = new JavaScriptSerializer();
        private readonly string runId;

        internal EventWriter(TextWriter output, string runId)
        {
            this.output = output;
            this.runId = runId ?? string.Empty;
        }

        internal void Write(string type, IDictionary<string, object> values)
        {
            Dictionary<string, object> payload = new Dictionary<string, object>();
            payload["protocolVersion"] = 1;
            payload["runId"] = runId;
            payload["type"] = type;
            if (values != null)
            {
                foreach (KeyValuePair<string, object> value in values)
                {
                    payload[value.Key] = value.Value;
                }
            }

            lock (gate)
            {
                string json = serializer.Serialize(payload);
                output.WriteLine(json);
                output.Flush();
            }
        }

        internal void Error(string code, string message, bool inputStreamEnded)
        {
            Write("error", Values(
                "ok", false,
                "code", code,
                "message", message,
                "inputStreamEnded", inputStreamEnded));
        }

        internal static Dictionary<string, object> Values(params object[] pairs)
        {
            Dictionary<string, object> values = new Dictionary<string, object>();
            for (int index = 0; index < pairs.Length; index += 2)
            {
                values[(string)pairs[index]] = pairs[index + 1];
            }
            return values;
        }
    }

    internal sealed class FinalAccumulator
    {
        private readonly object gate = new object();
        private string transcript = string.Empty;
        private bool completed;

        internal void Append(string segment)
        {
            lock (gate)
            {
                if (completed)
                {
                    throw new InvalidOperationException("The final transcript is already complete.");
                }
                transcript = Program.JoinSegments(transcript, segment);
            }
        }

        internal bool TryComplete(out string text)
        {
            lock (gate)
            {
                text = transcript;
                if (completed)
                {
                    return false;
                }
                completed = true;
                return true;
            }
        }
    }

    internal static class Program
    {
        private const int ProtocolVersion = 1;

        private static int Main(string[] args)
        {
            Console.OutputEncoding = new UTF8Encoding(false);
            string tentativeRunId = FindRunId(args);
            EventWriter writer = new EventWriter(Console.Out, tentativeRunId);
            try
            {
                Options options = ParseArguments(args);
                writer = new EventWriter(Console.Out, options.RunId);
                if (options.Command == "self-test")
                {
                    return RunSelfTests(writer);
                }
                if (options.Command == "probe")
                {
                    return RunProbe(options, writer);
                }
                return RunRecognition(options, writer);
            }
            catch (ArgumentExceptionWithCode exception)
            {
                writer.Error("INVALID_ARGUMENT", exception.Message, false);
                Diagnostic("invalid arguments", exception);
                return 2;
            }
            catch (FileNotFoundException exception)
            {
                writer.Error("SYSTEM_SPEECH_UNAVAILABLE", "Windows speech recognition is unavailable.", false);
                Diagnostic("System.Speech unavailable", exception);
                return 3;
            }
            catch (Exception exception)
            {
                writer.Error("LOCAL_FAILURE", "Windows speech helper failed locally.", false);
                Diagnostic("local failure", exception);
                return 1;
            }
        }

        internal static Options ParseArguments(string[] args)
        {
            Options options = new Options();
            HashSet<string> seen = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            for (int index = 0; index < args.Length; index++)
            {
                string argument = args[index];
                if (argument == "--self-test" || argument == "--probe")
                {
                    if (options.Command != null)
                    {
                        throw new ArgumentExceptionWithCode("Specify exactly one command.");
                    }
                    options.Command = argument.Substring(2);
                    if (argument == "--probe" && index + 1 < args.Length && !args[index + 1].StartsWith("--", StringComparison.Ordinal))
                    {
                        options.Language = args[++index];
                    }
                    continue;
                }

                string value = RequireValue(args, ref index, argument);
                if (!seen.Add(argument))
                {
                    throw new ArgumentExceptionWithCode("Duplicate argument: " + argument);
                }
                switch (argument)
                {
                    case "--mode":
                        if (options.Command != null)
                        {
                            throw new ArgumentExceptionWithCode("Specify exactly one command.");
                        }
                        options.Command = "recognize";
                        options.Mode = value;
                        break;
                    case "--run-id":
                        options.RunId = value;
                        break;
                    case "--language":
                        options.Language = value;
                        break;
                    case "--sample-rate":
                        options.SampleRate = ParseInteger(argument, value);
                        break;
                    case "--channels":
                        options.Channels = ParseInteger(argument, value);
                        break;
                    case "--bits":
                        options.Bits = ParseInteger(argument, value);
                        break;
                    default:
                        throw new ArgumentExceptionWithCode("Unknown argument: " + argument);
                }
            }

            if (options.Command == null)
            {
                throw new ArgumentExceptionWithCode("One of --self-test, --probe, or --mode is required.");
            }
            if (string.IsNullOrWhiteSpace(options.RunId))
            {
                throw new ArgumentExceptionWithCode("--run-id is required.");
            }
            if (options.Command == "recognize" && options.Mode != "stream" && options.Mode != "batch")
            {
                throw new ArgumentExceptionWithCode("--mode must be stream or batch.");
            }
            if (options.Language != "follow-windows" &&
                !options.Language.Equals("zh-CN", StringComparison.OrdinalIgnoreCase) &&
                !options.Language.Equals("en-US", StringComparison.OrdinalIgnoreCase))
            {
                throw new ArgumentExceptionWithCode("--language must be follow-windows, zh-CN, or en-US.");
            }
            if (options.SampleRate != 16000 || options.Channels != 1 || options.Bits != 16)
            {
                throw new ArgumentExceptionWithCode("PCM format must be 16000 Hz, mono, 16-bit.");
            }
            return options;
        }

        private static string RequireValue(string[] args, ref int index, string argument)
        {
            if (!argument.StartsWith("--", StringComparison.Ordinal) || index + 1 >= args.Length || args[index + 1].StartsWith("--", StringComparison.Ordinal))
            {
                throw new ArgumentExceptionWithCode("Missing value for argument: " + argument);
            }
            return args[++index];
        }

        private static int ParseInteger(string name, string value)
        {
            int parsed;
            if (!int.TryParse(value, NumberStyles.None, CultureInfo.InvariantCulture, out parsed))
            {
                throw new ArgumentExceptionWithCode("Invalid integer for " + name + ".");
            }
            return parsed;
        }

        private static string FindRunId(string[] args)
        {
            for (int index = 0; index + 1 < args.Length; index++)
            {
                if (args[index] == "--run-id")
                {
                    return args[index + 1];
                }
            }
            return string.Empty;
        }

        internal static string ResolveLanguage(string requested, string currentUiLanguage, IEnumerable<string> catalog)
        {
            List<string> installed = catalog.Where(item => !string.IsNullOrWhiteSpace(item)).ToList();
            if (!requested.Equals("follow-windows", StringComparison.OrdinalIgnoreCase))
            {
                return installed.FirstOrDefault(item => item.Equals(requested, StringComparison.OrdinalIgnoreCase));
            }

            string exact = installed.FirstOrDefault(item => item.Equals(currentUiLanguage, StringComparison.OrdinalIgnoreCase));
            if (exact != null)
            {
                return exact;
            }

            string neutral;
            try
            {
                neutral = CultureInfo.GetCultureInfo(currentUiLanguage).TwoLetterISOLanguageName;
            }
            catch (CultureNotFoundException)
            {
                return null;
            }
            return installed.FirstOrDefault(item =>
            {
                try
                {
                    return CultureInfo.GetCultureInfo(item).TwoLetterISOLanguageName.Equals(neutral, StringComparison.OrdinalIgnoreCase);
                }
                catch (CultureNotFoundException)
                {
                    return false;
                }
            });
        }

        internal static string JoinSegments(string left, string right)
        {
            if (string.IsNullOrEmpty(left))
            {
                return right ?? string.Empty;
            }
            if (string.IsNullOrEmpty(right))
            {
                return left;
            }
            char last = left[left.Length - 1];
            char first = right[0];
            bool addSpace = IsLatinLetterOrDigit(last) && IsLatinLetterOrDigit(first);
            return left + (addSpace ? " " : string.Empty) + right;
        }

        private static bool IsLatinLetterOrDigit(char value)
        {
            return (value >= 'A' && value <= 'Z') ||
                   (value >= 'a' && value <= 'z') ||
                   (value >= '0' && value <= '9');
        }

        private static List<RecognizerInfo> InstalledRecognizers()
        {
            return SpeechRecognitionEngine.InstalledRecognizers().ToList();
        }

        private static RecognizerInfo ResolveRecognizer(string language, IEnumerable<RecognizerInfo> catalog)
        {
            List<RecognizerInfo> recognizers = catalog.ToList();
            string resolved = ResolveLanguage(language, CultureInfo.CurrentUICulture.Name, recognizers.Select(item => item.Culture.Name));
            return recognizers.FirstOrDefault(item => item.Culture.Name.Equals(resolved, StringComparison.OrdinalIgnoreCase));
        }

        private static int RunProbe(Options options, EventWriter writer)
        {
            try
            {
                List<RecognizerInfo> installed = InstalledRecognizers();
                RecognizerInfo recognizer = ResolveRecognizer(options.Language, installed);
                if (recognizer == null)
                {
                    writer.Error("RECOGNIZER_MISSING", "The requested Windows speech recognizer is not installed.", false);
                    return 4;
                }

                using (SpeechRecognitionEngine engine = new SpeechRecognitionEngine(recognizer))
                {
                    try
                    {
                        engine.LoadGrammar(new DictationGrammar());
                    }
                    catch (Exception exception)
                    {
                        writer.Error("GRAMMAR_LOAD_FAILED", "The dictation grammar could not be loaded.", false);
                        Diagnostic("grammar load failed", exception);
                        return 5;
                    }
                }

                writer.Write("probe", EventWriter.Values(
                    "ok", true,
                    "resolvedLanguage", recognizer.Culture.Name,
                    "installedLanguages", installed.Select(item => item.Culture.Name).Distinct(StringComparer.OrdinalIgnoreCase).ToArray()));
                return 0;
            }
            catch (Exception exception)
            {
                writer.Error("SYSTEM_SPEECH_UNAVAILABLE", "Windows speech recognition is unavailable.", false);
                Diagnostic("recognizer probe failed", exception);
                return 3;
            }
        }

        private static int RunRecognition(Options options, EventWriter writer)
        {
            List<RecognizerInfo> installed;
            try
            {
                installed = InstalledRecognizers();
            }
            catch (Exception exception)
            {
                writer.Error("SYSTEM_SPEECH_UNAVAILABLE", "Windows speech recognition is unavailable.", false);
                Diagnostic("recognizer enumeration failed", exception);
                return 3;
            }

            RecognizerInfo recognizer = ResolveRecognizer(options.Language, installed);
            if (recognizer == null)
            {
                writer.Error("RECOGNIZER_MISSING", "The requested Windows speech recognizer is not installed.", false);
                return 4;
            }

            using (ProducerConsumerAudioStream audio = new ProducerConsumerAudioStream())
            using (SpeechRecognitionEngine engine = new SpeechRecognitionEngine(recognizer))
            using (ManualResetEvent completed = new ManualResetEvent(false))
            {
                FinalAccumulator accumulator = new FinalAccumulator();
                Exception producerFailure = null;
                Exception recognitionFailure = null;
                bool recognitionCancelled = false;
                bool inputStreamEnded = false;

                try
                {
                    engine.LoadGrammar(new DictationGrammar());
                }
                catch (Exception exception)
                {
                    writer.Error("GRAMMAR_LOAD_FAILED", "The dictation grammar could not be loaded.", false);
                    Diagnostic("grammar load failed", exception);
                    return 5;
                }

                engine.SpeechHypothesized += delegate(object sender, SpeechHypothesizedEventArgs eventArgs)
                {
                    if (eventArgs.Result != null && !string.IsNullOrWhiteSpace(eventArgs.Result.Text))
                    {
                        writer.Write("hypothesis", EventWriter.Values("ok", true, "text", eventArgs.Result.Text));
                    }
                };
                engine.SpeechRecognized += delegate(object sender, SpeechRecognizedEventArgs eventArgs)
                {
                    if (eventArgs.Result != null && !string.IsNullOrWhiteSpace(eventArgs.Result.Text))
                    {
                        accumulator.Append(eventArgs.Result.Text);
                        writer.Write("recognized", EventWriter.Values(
                            "ok", true,
                            "text", eventArgs.Result.Text,
                            "confidence", eventArgs.Result.Confidence));
                    }
                };
                engine.RecognizeCompleted += delegate(object sender, RecognizeCompletedEventArgs eventArgs)
                {
                    recognitionFailure = eventArgs.Error;
                    recognitionCancelled = eventArgs.Cancelled;
                    completed.Set();
                };

                try
                {
                    SpeechAudioFormatInfo format = new SpeechAudioFormatInfo(
                        options.SampleRate,
                        AudioBitsPerSample.Sixteen,
                        AudioChannel.Mono);
                    engine.SetInputToAudioStream(audio, format);
                }
                catch (Exception exception)
                {
                    writer.Error("PCM_FORMAT_UNSUPPORTED", "The raw PCM format is unsupported.", false);
                    Diagnostic("PCM format rejected", exception);
                    return 6;
                }

                Thread producer = new Thread(delegate()
                {
                    try
                    {
                        Stream input = Console.OpenStandardInput();
                        byte[] buffer = new byte[8192];
                        int read;
                        while ((read = input.Read(buffer, 0, buffer.Length)) > 0)
                        {
                            audio.WriteChunk(buffer, 0, read);
                        }
                        inputStreamEnded = true;
                        audio.CompleteWriting();
                    }
                    catch (Exception exception)
                    {
                        producerFailure = exception;
                        audio.Cancel();
                        try
                        {
                            engine.RecognizeAsyncCancel();
                        }
                        catch (InvalidOperationException)
                        {
                        }
                    }
                });
                producer.IsBackground = true;
                producer.Name = "windows-speech-pcm-producer";

                writer.Write("ready", EventWriter.Values(
                    "ok", true,
                    "resolvedLanguage", recognizer.Culture.Name,
                    "mode", options.Mode));
                engine.RecognizeAsync(RecognizeMode.Multiple);
                producer.Start();
                completed.WaitOne();
                producer.Join();

                if (producerFailure is InputTooLargeException)
                {
                    writer.Error("INPUT_TOO_LARGE", "Raw PCM input exceeds the 64 MiB limit.", inputStreamEnded);
                    return 7;
                }
                if (producerFailure is OperationCanceledException || recognitionCancelled)
                {
                    writer.Error("CANCELLED", "Speech recognition was cancelled.", inputStreamEnded);
                    return 8;
                }
                if (producerFailure != null || recognitionFailure != null)
                {
                    writer.Error("LOCAL_FAILURE", "Windows speech recognition failed locally.", inputStreamEnded);
                    Diagnostic("recognition failed", producerFailure ?? recognitionFailure);
                    return 1;
                }

                string transcript;
                if (!accumulator.TryComplete(out transcript))
                {
                    writer.Error("LOCAL_FAILURE", "Final transcript was emitted more than once.", inputStreamEnded);
                    return 1;
                }
                if (string.IsNullOrWhiteSpace(transcript))
                {
                    writer.Error("NO_SPEECH", "No speech was recognized.", inputStreamEnded);
                    return 9;
                }
                writer.Write("final", EventWriter.Values(
                    "ok", true,
                    "text", transcript,
                    "inputStreamEnded", inputStreamEnded));
                return 0;
            }
        }

        private static int RunSelfTests(EventWriter writer)
        {
            List<string> passed = new List<string>();
            try
            {
                TestArgumentsAndProtocol(); passed.Add("arguments-and-protocol");
                TestEventJson(); passed.Add("event-json");
                TestLanguageResolver(); passed.Add("language-resolver");
                TestSegmentJoin(); passed.Add("segment-join");
                TestFinalAccumulator(); passed.Add("exact-one-final");
                TestStreamOrderEofAndSeek(); passed.Add("stream-order-eof-seek");
                TestStreamLimit(); passed.Add("stream-64mib-limit");
                TestBackpressure(); passed.Add("stream-backpressure");
                TestCompleteWakeup(); passed.Add("stream-complete-wakeup");
                TestCancelWakeups(); passed.Add("stream-cancel-wakeups");
                TestConcurrentEventWriter(); passed.Add("concurrent-event-json");
                writer.Write("self-test", EventWriter.Values("ok", true, "tests", passed.ToArray()));
                return 0;
            }
            catch (Exception exception)
            {
                writer.Write("self-test", EventWriter.Values(
                    "ok", false,
                    "failedAfter", passed.ToArray(),
                    "message", exception.GetType().Name + ": " + exception.Message));
                Diagnostic("self-test failure", exception);
                return 10;
            }
        }

        private static void TestArgumentsAndProtocol()
        {
            Options parsed = ParseArguments(new[] { "--self-test", "--run-id", "test" });
            Assert(parsed.Command == "self-test" && parsed.RunId == "test", "self-test arguments were not parsed");
            Expect<ArgumentExceptionWithCode>(delegate { ParseArguments(new[] { "--self-test" }); });
            Expect<ArgumentExceptionWithCode>(delegate { ParseArguments(new[] { "--mode", "stream", "--run-id", "x", "--sample-rate", "8000" }); });
            StringWriter output = new StringWriter(CultureInfo.InvariantCulture);
            new EventWriter(output, "protocol").Write("ready", EventWriter.Values("ok", true));
            Dictionary<string, object> json = DeserializeLine(output.ToString());
            Assert(Convert.ToInt32(json["protocolVersion"], CultureInfo.InvariantCulture) == ProtocolVersion, "wrong protocol version");
            Assert((string)json["runId"] == "protocol" && (string)json["type"] == "ready", "protocol envelope is incomplete");
        }

        private static void TestEventJson()
        {
            StringWriter output = new StringWriter(CultureInfo.InvariantCulture);
            new EventWriter(output, "quoted").Write("recognized", EventWriter.Values("text", "中\"文\nline"));
            Dictionary<string, object> json = DeserializeLine(output.ToString());
            Assert((string)json["text"] == "中\"文\nline", "event text did not survive JSON serialization");
        }

        private static void TestLanguageResolver()
        {
            string[] catalog = { "en-US", "zh-CN" };
            Assert(ResolveLanguage("ZH-cn", "en-US", catalog) == "zh-CN", "exact resolver must be case insensitive");
            Assert(ResolveLanguage("follow-windows", "en-US", catalog) == "en-US", "follow-windows exact match failed");
            Assert(ResolveLanguage("follow-windows", "zh-HK", catalog) == "zh-CN", "follow-windows neutral match failed");
            Assert(ResolveLanguage("follow-windows", "fr-FR", catalog) == null, "resolver must not silently use another language");
        }

        private static void TestSegmentJoin()
        {
            Assert(JoinSegments("hello", "world") == "hello world", "Latin segments need one space");
            Assert(JoinSegments("hello ", "world") == "hello world", "existing spaces must not be duplicated");
            Assert(JoinSegments("你好", "世界") == "你好世界", "CJK segments must not gain a space");
            Assert(JoinSegments("你好", "world") == "你好world", "mixed boundary must not gain a space");
            Assert(JoinSegments("abc1", "2def") == "abc1 2def", "digits are Latin boundaries");
        }

        private static void TestFinalAccumulator()
        {
            FinalAccumulator accumulator = new FinalAccumulator();
            accumulator.Append("hello");
            accumulator.Append("world");
            string text;
            Assert(accumulator.TryComplete(out text) && text == "hello world", "first final was incorrect");
            Assert(!accumulator.TryComplete(out text), "second final must be rejected");
            Expect<InvalidOperationException>(delegate { accumulator.Append("late"); });
        }

        private static void TestStreamOrderEofAndSeek()
        {
            using (ProducerConsumerAudioStream stream = new ProducerConsumerAudioStream(8))
            {
                Assert(stream.CanRead && stream.CanSeek && !stream.CanWrite, "stream compatibility flags are wrong");
                Assert(stream.Length == ProducerConsumerAudioStream.DeclaredLength && stream.Position == 0, "stream length or position is wrong");
                stream.WriteChunk(new byte[] { 1, 2, 3 }, 0, 3);
                stream.CompleteWriting();
                byte[] output = new byte[4];
                Assert(stream.Read(output, 0, output.Length) == 3, "EOF must allow a short read");
                Assert(output[0] == 1 && output[1] == 2 && output[2] == 3, "stream byte order changed");
                Assert(stream.Read(output, 0, output.Length) == 0, "EOF must return zero");
                Assert(stream.Seek(0, SeekOrigin.Current) == 3, "Seek(0, Current) must report position");
                stream.Position = 3;
                Expect<NotSupportedException>(delegate { stream.Position = 0; });
                Expect<NotSupportedException>(delegate { stream.Seek(0, SeekOrigin.Begin); });
                Expect<NotSupportedException>(delegate { stream.Write(output, 0, 1); });
                Expect<NotSupportedException>(delegate { stream.SetLength(1); });
            }
        }

        private static void TestStreamLimit()
        {
            using (ProducerConsumerAudioStream stream = new ProducerConsumerAudioStream(32768))
            {
                byte[] block = new byte[32768];
                byte[] read = new byte[32768];
                int iterations = (int)(ProducerConsumerAudioStream.DeclaredLength / block.Length);
                for (int index = 0; index < iterations; index++)
                {
                    stream.WriteChunk(block, 0, block.Length);
                    Assert(stream.Read(read, 0, read.Length) == read.Length, "limit test read was short");
                }
                Expect<InputTooLargeException>(delegate { stream.WriteChunk(new byte[1], 0, 1); });
            }
        }

        private static void TestBackpressure()
        {
            using (ProducerConsumerAudioStream stream = new ProducerConsumerAudioStream())
            {
                Exception failure = null;
                Thread writer = new Thread(delegate()
                {
                    try { stream.WriteChunk(new byte[ProducerConsumerAudioStream.DefaultCapacity + 1], 0, ProducerConsumerAudioStream.DefaultCapacity + 1); }
                    catch (Exception exception) { failure = exception; }
                });
                writer.Start();
                Assert(!writer.Join(100), "writer did not apply backpressure at 64,000 bytes");
                byte[] one = new byte[1];
                Assert(stream.Read(one, 0, 1) == 1, "backpressure release read failed");
                Assert(writer.Join(2000), "writer did not recover after capacity was released");
                Assert(failure == null, "writer failed after backpressure recovery");
                stream.Cancel();
            }
        }

        private static void TestCancelWakeups()
        {
            using (ProducerConsumerAudioStream readerStream = new ProducerConsumerAudioStream())
            {
                int readResult = -1;
                Thread reader = new Thread(new ThreadStart(delegate { readResult = readerStream.Read(new byte[1], 0, 1); }));
                reader.Start();
                Assert(!reader.Join(100), "empty reader was not waiting");
                readerStream.Cancel();
                Assert(reader.Join(2000) && readResult == 0, "cancel did not wake reader");
            }

            using (ProducerConsumerAudioStream writerStream = new ProducerConsumerAudioStream())
            {
                Exception failure = null;
                Thread writer = new Thread(delegate()
                {
                    try { writerStream.WriteChunk(new byte[ProducerConsumerAudioStream.DefaultCapacity + 1], 0, ProducerConsumerAudioStream.DefaultCapacity + 1); }
                    catch (Exception exception) { failure = exception; }
                });
                writer.Start();
                Assert(!writer.Join(100), "full writer was not waiting");
                writerStream.Cancel();
                Assert(writer.Join(2000) && failure is OperationCanceledException, "cancel did not wake writer");
            }
        }

        private static void TestCompleteWakeup()
        {
            using (ProducerConsumerAudioStream readerStream = new ProducerConsumerAudioStream())
            {
                int readResult = -1;
                Thread reader = new Thread(new ThreadStart(delegate { readResult = readerStream.Read(new byte[1], 0, 1); }));
                reader.Start();
                Assert(!reader.Join(100), "empty reader was not waiting before completion");
                readerStream.CompleteWriting();
                Assert(reader.Join(2000) && readResult == 0, "completion did not wake an empty reader at EOF");
            }

            using (ProducerConsumerAudioStream stream = new ProducerConsumerAudioStream())
            {
                Exception failure = null;
                Thread writer = new Thread(delegate()
                {
                    try { stream.WriteChunk(new byte[ProducerConsumerAudioStream.DefaultCapacity + 1], 0, ProducerConsumerAudioStream.DefaultCapacity + 1); }
                    catch (Exception exception) { failure = exception; }
                });
                writer.Start();
                Assert(!writer.Join(100), "full writer was not waiting before completion");
                stream.CompleteWriting();
                bool woke = writer.Join(2000);
                stream.Cancel();
                Assert(woke && failure is InvalidOperationException, "completion did not wake and reject a waiting writer");
            }
        }

        private static void TestConcurrentEventWriter()
        {
            StringWriter output = new StringWriter(CultureInfo.InvariantCulture);
            EventWriter writer = new EventWriter(output, "concurrent");
            List<Thread> threads = new List<Thread>();
            for (int threadIndex = 0; threadIndex < 8; threadIndex++)
            {
                int captured = threadIndex;
                Thread thread = new Thread(delegate()
                {
                    for (int eventIndex = 0; eventIndex < 50; eventIndex++)
                    {
                        writer.Write("hypothesis", EventWriter.Values("text", captured + ":" + eventIndex));
                    }
                });
                threads.Add(thread);
                thread.Start();
            }
            foreach (Thread thread in threads)
            {
                Assert(thread.Join(5000), "concurrent event writer deadlocked");
            }
            string[] lines = output.ToString().Split(new[] { Environment.NewLine }, StringSplitOptions.RemoveEmptyEntries);
            Assert(lines.Length == 400, "concurrent event count changed");
            foreach (string line in lines)
            {
                Dictionary<string, object> json = DeserializeLine(line);
                Assert((string)json["type"] == "hypothesis", "concurrent line was not one complete event");
            }
        }

        private static Dictionary<string, object> DeserializeLine(string line)
        {
            return new JavaScriptSerializer().Deserialize<Dictionary<string, object>>(line.Trim());
        }

        private static void Assert(bool condition, string message)
        {
            if (!condition)
            {
                throw new InvalidOperationException(message);
            }
        }

        private static void Expect<TException>(ThreadStart action) where TException : Exception
        {
            try
            {
                action();
            }
            catch (TException)
            {
                return;
            }
            throw new InvalidOperationException("Expected " + typeof(TException).Name + ".");
        }

        private static void Diagnostic(string context, Exception exception)
        {
            Console.Error.WriteLine("[windows-speech] {0}: {1}: {2}", context, exception.GetType().Name, exception.Message);
        }
    }
}
