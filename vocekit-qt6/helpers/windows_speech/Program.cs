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

        internal void Error(string errorCode, string message, bool inputStreamEnded)
        {
            Write("error", Values(
                "ok", false,
                "errorCode", errorCode,
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

    internal sealed class BlockingReadStream : Stream
    {
        private readonly ManualResetEvent readStarted = new ManualResetEvent(false);
        private readonly ManualResetEvent released = new ManualResetEvent(false);
        private bool disposed;

        internal bool WasDisposed
        {
            get { return disposed; }
        }

        internal bool WaitUntilReadStarted(int milliseconds)
        {
            return readStarted.WaitOne(milliseconds);
        }

        public override bool CanRead { get { return true; } }
        public override bool CanSeek { get { return false; } }
        public override bool CanWrite { get { return false; } }
        public override long Length { get { throw new NotSupportedException(); } }
        public override long Position
        {
            get { throw new NotSupportedException(); }
            set { throw new NotSupportedException(); }
        }

        public override int Read(byte[] buffer, int offset, int count)
        {
            readStarted.Set();
            released.WaitOne();
            if (disposed)
            {
                throw new ObjectDisposedException("BlockingReadStream");
            }
            return 0;
        }

        protected override void Dispose(bool disposing)
        {
            if (disposing && !disposed)
            {
                disposed = true;
                released.Set();
            }
            base.Dispose(disposing);
        }

        public override void Flush() { }
        public override long Seek(long offset, SeekOrigin origin) { throw new NotSupportedException(); }
        public override void SetLength(long value) { throw new NotSupportedException(); }
        public override void Write(byte[] buffer, int offset, int count) { throw new NotSupportedException(); }
    }

    internal sealed class ThrowingReadStream : Stream
    {
        private readonly string message;

        internal ThrowingReadStream(string message)
        {
            this.message = message;
        }

        public override bool CanRead { get { return true; } }
        public override bool CanSeek { get { return false; } }
        public override bool CanWrite { get { return false; } }
        public override long Length { get { throw new NotSupportedException(); } }
        public override long Position
        {
            get { throw new NotSupportedException(); }
            set { throw new NotSupportedException(); }
        }

        public override int Read(byte[] buffer, int offset, int count)
        {
            throw new IOException(message);
        }

        public override void Flush() { }
        public override long Seek(long offset, SeekOrigin origin) { throw new NotSupportedException(); }
        public override void SetLength(long value) { throw new NotSupportedException(); }
        public override void Write(byte[] buffer, int offset, int count) { throw new NotSupportedException(); }
    }

    internal sealed class InputPump : IDisposable
    {
        private const int StopWaitMilliseconds = 1000;
        private readonly Stream input;
        private readonly ProducerConsumerAudioStream audio;
        private readonly Action cancelRecognizer;
        private readonly Thread thread;
        private readonly bool isBackground;
        private readonly ManualResetEvent stopped = new ManualResetEvent(false);
        private volatile bool stopRequested;
        private volatile bool inputStreamEnded;
        private Exception failure;
        private bool started;
        private bool disposed;

        internal InputPump(Stream input, ProducerConsumerAudioStream audio, Action cancelRecognizer)
        {
            if (input == null) { throw new ArgumentNullException("input"); }
            if (audio == null) { throw new ArgumentNullException("audio"); }
            if (cancelRecognizer == null) { throw new ArgumentNullException("cancelRecognizer"); }
            this.input = input;
            this.audio = audio;
            this.cancelRecognizer = cancelRecognizer;
            thread = new Thread(Pump);
            isBackground = true;
            thread.IsBackground = isBackground;
            thread.Name = "windows-speech-pcm-producer";
        }

        internal bool InputStreamEnded
        {
            get { return inputStreamEnded; }
        }

        internal Exception Failure
        {
            get { return failure; }
        }

        internal bool IsBackground
        {
            get { return isBackground; }
        }

        internal bool IsStopped
        {
            get { return stopped.WaitOne(0); }
        }

        internal void Start()
        {
            if (disposed) { throw new ObjectDisposedException("InputPump"); }
            if (started) { throw new InvalidOperationException("The input pump has already started."); }
            started = true;
            thread.Start();
        }

        internal void StopAfterRecognizerCompleted()
        {
            stopRequested = true;
            audio.Cancel();
            try
            {
                input.Dispose();
            }
            catch (Exception)
            {
            }
            if (started)
            {
                stopped.WaitOne(StopWaitMilliseconds);
            }
        }

        internal bool WaitForStop(int milliseconds)
        {
            return !started || stopped.WaitOne(milliseconds);
        }

        public void Dispose()
        {
            if (disposed)
            {
                return;
            }
            disposed = true;
            if (!IsStopped)
            {
                StopAfterRecognizerCompleted();
            }
            else
            {
                try { input.Dispose(); } catch (Exception) { }
            }
            if (IsStopped)
            {
                stopped.Dispose();
            }
        }

        private void Pump()
        {
            try
            {
                byte[] buffer = new byte[8192];
                int read;
                while ((read = input.Read(buffer, 0, buffer.Length)) > 0)
                {
                    audio.WriteChunk(buffer, 0, read);
                }
                if (!stopRequested)
                {
                    inputStreamEnded = true;
                    audio.CompleteWriting();
                }
            }
            catch (Exception exception)
            {
                if (!stopRequested)
                {
                    failure = exception;
                    audio.Cancel();
                    try { cancelRecognizer(); } catch (InvalidOperationException) { }
                }
            }
            finally
            {
                stopped.Set();
            }
        }
    }

    internal static class Program
    {
        private const int ProtocolVersion = 1;

        private static int Main(string[] args)
        {
            Console.OutputEncoding = new UTF8Encoding(false);
            if (args.Length == 1 && args[0] == "--build-provenance-json")
            {
                return WriteBuildProvenance();
            }
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

        private static int WriteBuildProvenance()
        {
            Dictionary<string, object> provenance = new Dictionary<string, object>();
            provenance["schema_version"] = BuildProvenance.SchemaVersion;
            provenance["kind"] = BuildProvenance.Kind;
            provenance["helper_name"] = BuildProvenance.HelperName;
            provenance["source_commit"] = BuildProvenance.SourceCommit;
            provenance["source_tree_clean"] = BuildProvenance.SourceTreeClean;
            provenance["configuration"] = BuildProvenance.Configuration;
            Console.Out.WriteLine(new JavaScriptSerializer().Serialize(provenance));
            return 0;
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
                Exception recognitionFailure = null;
                bool recognitionCancelled = false;

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

                using (InputPump inputPump = new InputPump(
                    Console.OpenStandardInput(),
                    audio,
                    delegate
                    {
                        engine.RecognizeAsyncCancel();
                    }))
                {
                    writer.Write("ready", EventWriter.Values(
                        "ok", true,
                        "resolvedLanguage", recognizer.Culture.Name,
                        "mode", options.Mode));
                    engine.RecognizeAsync(RecognizeMode.Multiple);
                    inputPump.Start();
                    completed.WaitOne();
                    inputPump.StopAfterRecognizerCompleted();

                    Exception producerFailure = inputPump.Failure;
                    bool inputStreamEnded = inputPump.InputStreamEnded;
                    string errorCode;
                    int errorExitCode = ClassifyRecognitionFailure(
                        producerFailure,
                        recognitionCancelled,
                        recognitionFailure,
                        inputStreamEnded,
                        out errorCode);
                    if (errorExitCode != 0)
                    {
                        string message = errorCode == "INPUT_TOO_LARGE"
                            ? "Raw PCM input exceeds the 64 MiB limit."
                            : errorCode == "CANCELLED"
                                ? "Speech recognition was cancelled."
                                : "Windows speech recognition failed locally.";
                        writer.Error(errorCode, message, inputStreamEnded);
                        if (errorCode == "LOCAL_FAILURE")
                        {
                            Diagnostic("recognition failed", producerFailure ?? recognitionFailure);
                        }
                        return errorExitCode;
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
        }

        private static int ClassifyRecognitionFailure(
            Exception producerFailure,
            bool recognitionCancelled,
            Exception recognitionFailure,
            bool inputStreamEnded,
            out string errorCode)
        {
            if (producerFailure is InputTooLargeException)
            {
                errorCode = "INPUT_TOO_LARGE";
                return 7;
            }
            if (producerFailure is OperationCanceledException)
            {
                errorCode = "CANCELLED";
                return 8;
            }
            if (producerFailure != null)
            {
                errorCode = "LOCAL_FAILURE";
                return 1;
            }
            if (recognitionCancelled)
            {
                errorCode = "CANCELLED";
                return 8;
            }
            if (recognitionFailure != null)
            {
                errorCode = "LOCAL_FAILURE";
                return 1;
            }
            if (!inputStreamEnded)
            {
                errorCode = "LOCAL_FAILURE";
                return 1;
            }
            errorCode = null;
            return 0;
        }

        private static int RunSelfTests(EventWriter writer)
        {
            List<string> passed = new List<string>();
            List<string> failures = new List<string>();
            try
            {
                RunSelfTestCase("arguments-and-protocol", TestArgumentsAndProtocol, passed, failures);
                RunSelfTestCase("event-json", TestEventJson, passed, failures);
                RunSelfTestCase("error-event-protocol", TestErrorEventProtocol, passed, failures);
                RunSelfTestCase("early-recognizer-completion", TestEarlyRecognizerCompletion, passed, failures);
                RunSelfTestCase("producer-failure-precedence", TestProducerFailurePrecedence, passed, failures);
                RunSelfTestCase("diagnostic-privacy", TestDiagnosticPrivacy, passed, failures);
                RunSelfTestCase("language-resolver", TestLanguageResolver, passed, failures);
                RunSelfTestCase("segment-join", TestSegmentJoin, passed, failures);
                RunSelfTestCase("exact-one-final", TestFinalAccumulator, passed, failures);
                RunSelfTestCase("stream-order-eof-seek", TestStreamOrderEofAndSeek, passed, failures);
                RunSelfTestCase("stream-64mib-limit", TestStreamLimit, passed, failures);
                RunSelfTestCase("stream-backpressure", TestBackpressure, passed, failures);
                RunSelfTestCase("stream-complete-wakeup", TestCompleteWakeup, passed, failures);
                RunSelfTestCase("stream-cancel-wakeups", TestCancelWakeups, passed, failures);
                RunSelfTestCase("stream-zero-count-state", TestZeroCountStateValidation, passed, failures);
                RunSelfTestCase("concurrent-event-json", TestConcurrentEventWriter, passed, failures);
                if (failures.Count != 0)
                {
                    throw new InvalidOperationException(string.Join(" | ", failures.ToArray()));
                }
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

        private static void RunSelfTestCase(string name, ThreadStart test, ICollection<string> passed, ICollection<string> failures)
        {
            try
            {
                test();
                passed.Add(name);
            }
            catch (Exception exception)
            {
                failures.Add(name + ": " + exception.GetType().Name + ": " + exception.Message);
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

        private static void TestErrorEventProtocol()
        {
            StringWriter output = new StringWriter(CultureInfo.InvariantCulture);
            new EventWriter(output, "error-contract").Error("INVALID_ARGUMENT", "invalid", false);
            Dictionary<string, object> json = DeserializeLine(output.ToString());
            Assert(json.ContainsKey("errorCode"), "error event is missing the required errorCode field");
            Assert((string)json["errorCode"] == "INVALID_ARGUMENT", "errorCode value changed");
            Assert(!json.ContainsKey("code"), "legacy code field must not be emitted");
        }

        private static void TestEarlyRecognizerCompletion()
        {
            using (BlockingReadStream input = new BlockingReadStream())
            using (ProducerConsumerAudioStream audio = new ProducerConsumerAudioStream())
            using (InputPump pump = new InputPump(input, audio, delegate { throw new InvalidOperationException("recognizer already completed"); }))
            {
                pump.Start();
                Assert(input.WaitUntilReadStarted(2000), "fake stdin pump did not enter its blocking read");
                System.Diagnostics.Stopwatch timer = System.Diagnostics.Stopwatch.StartNew();
                pump.StopAfterRecognizerCompleted();
                timer.Stop();

                Assert(timer.ElapsedMilliseconds < 1500, "recognizer completion waited indefinitely for blocked stdin");
                Assert(pump.IsBackground, "stdin pump must never be a foreground thread");
                Assert(pump.IsStopped, "disposing blocked stdin did not stop the fake pump");
                Assert(!pump.InputStreamEnded, "recognizer cancellation must not be reported as stdin EOF");
                Assert(input.WasDisposed, "recognizer completion did not release stdin");

                System.Reflection.MethodInfo classifier = typeof(Program).GetMethod(
                    "ClassifyRecognitionFailure",
                    System.Reflection.BindingFlags.Static | System.Reflection.BindingFlags.NonPublic);
                Assert(classifier != null, "recognition outcome classifier is missing");
                object[] arguments = { null, false, null, false, null };
                int exitCode = (int)classifier.Invoke(null, arguments);
                Assert(exitCode == 1, "recognizer completion before stdin EOF must fail");
                Assert((string)arguments[4] == "LOCAL_FAILURE", "early completion must use the stable local failure code");
            }
        }

        private static void TestProducerFailurePrecedence()
        {
            const string secret = "sensitive-input-source-detail";
            using (ThrowingReadStream input = new ThrowingReadStream(secret))
            using (ProducerConsumerAudioStream audio = new ProducerConsumerAudioStream())
            {
                int cancelRequests = 0;
                using (InputPump pump = new InputPump(input, audio, delegate { Interlocked.Increment(ref cancelRequests); }))
                {
                    pump.Start();
                    Assert(pump.WaitForStop(2000), "throwing input pump did not stop");
                    Assert(pump.Failure is IOException, "throwing input did not preserve its producer failure type");
                    Assert(cancelRequests == 1, "producer failure did not request recognizer cancellation exactly once");

                    System.Reflection.MethodInfo classifier = typeof(Program).GetMethod(
                        "ClassifyRecognitionFailure",
                        System.Reflection.BindingFlags.Static | System.Reflection.BindingFlags.NonPublic);
                    Assert(classifier != null, "producer/recognizer outcome classifier is missing");
                    object[] arguments = { pump.Failure, true, null, false, null };
                    int exitCode = (int)classifier.Invoke(null, arguments);
                    Assert(exitCode == 1, "producer IOException plus recognizer cancellation must exit 1");
                    Assert((string)arguments[4] == "LOCAL_FAILURE", "producer IOException must take precedence over recognizer cancellation");
                }
            }
        }

        private static void TestDiagnosticPrivacy()
        {
            const string secret = "sensitive-input-source-detail";
            StringWriter captured = new StringWriter(CultureInfo.InvariantCulture);
            TextWriter previous = Console.Error;
            try
            {
                Console.SetError(captured);
                Diagnostic("input pump failed", new IOException(secret));
            }
            finally
            {
                Console.SetError(previous);
            }
            Assert(captured.ToString().IndexOf(secret, StringComparison.Ordinal) < 0,
                "stderr diagnostic leaked an exception message");
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

        private static void TestZeroCountStateValidation()
        {
            byte[] buffer = new byte[1];
            ProducerConsumerAudioStream disposedStream = new ProducerConsumerAudioStream();
            disposedStream.Dispose();
            Expect<ObjectDisposedException>(delegate { disposedStream.Read(buffer, 0, 0); });
            Expect<ObjectDisposedException>(delegate { disposedStream.WriteChunk(buffer, 0, 0); });

            using (ProducerConsumerAudioStream cancelledStream = new ProducerConsumerAudioStream())
            {
                cancelledStream.Cancel();
                Assert(cancelledStream.Read(buffer, 0, 0) == 0, "cancelled zero-count Read must preserve the existing zero result");
                Expect<OperationCanceledException>(delegate { cancelledStream.WriteChunk(buffer, 0, 0); });
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
            Console.Error.WriteLine("[windows-speech] {0}: {1}", context, exception.GetType().Name);
        }
    }
}
