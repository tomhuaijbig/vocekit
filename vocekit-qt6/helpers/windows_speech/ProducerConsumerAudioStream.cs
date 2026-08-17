using System;
using System.Collections.Generic;
using System.IO;
using System.Threading;

namespace VoceKit.WindowsSpeech
{
    internal sealed class InputTooLargeException : Exception
    {
        public InputTooLargeException()
            : base("Raw PCM input exceeds the 64 MiB limit.")
        {
        }
    }

    internal sealed class ProducerConsumerAudioStream : Stream
    {
        internal const int DefaultCapacity = 64000;
        internal const long DeclaredLength = 64L * 1024L * 1024L;

        private readonly object monitor = new object();
        private readonly Queue<byte[]> chunks = new Queue<byte[]>();
        private readonly int capacity;
        private int headOffset;
        private int bufferedBytes;
        private long acceptedBytes;
        private long readBytes;
        private bool writingCompleted;
        private bool cancelled;
        private bool disposed;

        internal ProducerConsumerAudioStream()
            : this(DefaultCapacity)
        {
        }

        internal ProducerConsumerAudioStream(int capacity)
        {
            if (capacity <= 0)
            {
                throw new ArgumentOutOfRangeException("capacity");
            }

            this.capacity = capacity;
        }

        public override bool CanRead
        {
            get { return true; }
        }

        public override bool CanSeek
        {
            get { return true; }
        }

        public override bool CanWrite
        {
            get { return false; }
        }

        public override long Length
        {
            get { return DeclaredLength; }
        }

        public override long Position
        {
            get
            {
                lock (monitor)
                {
                    return readBytes;
                }
            }
            set
            {
                lock (monitor)
                {
                    ThrowIfDisposed();
                    if (value != readBytes)
                    {
                        throw new NotSupportedException("Repositioning the live PCM stream is not supported.");
                    }
                }
            }
        }

        internal void WriteChunk(byte[] buffer, int offset, int count)
        {
            ValidateBufferArguments(buffer, offset, count);
            lock (monitor)
            {
                ThrowIfDisposed();
                if (cancelled)
                {
                    throw new OperationCanceledException("PCM input was cancelled.");
                }
                if (writingCompleted)
                {
                    throw new InvalidOperationException("PCM input has already completed.");
                }
                if (count == 0)
                {
                    return;
                }
                if (acceptedBytes > DeclaredLength - count)
                {
                    throw new InputTooLargeException();
                }

                // Reserve the whole caller chunk before waiting so concurrent writers
                // cannot race past the cumulative input limit.
                acceptedBytes += count;
            }

            int remaining = count;
            int sourceOffset = offset;
            try
            {
                while (remaining > 0)
                {
                    lock (monitor)
                    {
                        while (bufferedBytes == capacity && !writingCompleted && !cancelled && !disposed)
                        {
                            Monitor.Wait(monitor);
                        }

                        ThrowIfDisposed();
                        if (cancelled)
                        {
                            throw new OperationCanceledException("PCM input was cancelled.");
                        }
                        if (writingCompleted)
                        {
                            throw new InvalidOperationException("PCM input has already completed.");
                        }

                        int partLength = Math.Min(remaining, capacity - bufferedBytes);
                        byte[] part = new byte[partLength];
                        Buffer.BlockCopy(buffer, sourceOffset, part, 0, partLength);
                        chunks.Enqueue(part);
                        bufferedBytes += partLength;
                        sourceOffset += partLength;
                        remaining -= partLength;
                        Monitor.PulseAll(monitor);
                    }
                }
            }
            catch
            {
                lock (monitor)
                {
                    acceptedBytes -= remaining;
                }
                throw;
            }
        }

        internal void CompleteWriting()
        {
            lock (monitor)
            {
                ThrowIfDisposed();
                writingCompleted = true;
                Monitor.PulseAll(monitor);
            }
        }

        internal void Cancel()
        {
            lock (monitor)
            {
                cancelled = true;
                Monitor.PulseAll(monitor);
            }
        }

        public override int Read(byte[] buffer, int offset, int count)
        {
            ValidateBufferArguments(buffer, offset, count);
            int copied = 0;
            lock (monitor)
            {
                ThrowIfDisposed();
                if (count == 0)
                {
                    return 0;
                }
                while (copied < count)
                {
                    while (bufferedBytes == 0 && !writingCompleted && !cancelled && !disposed)
                    {
                        Monitor.Wait(monitor);
                    }

                    ThrowIfDisposed();
                    if (bufferedBytes == 0)
                    {
                        break;
                    }

                    byte[] head = chunks.Peek();
                    int available = head.Length - headOffset;
                    int copyLength = Math.Min(count - copied, available);
                    Buffer.BlockCopy(head, headOffset, buffer, offset + copied, copyLength);
                    copied += copyLength;
                    headOffset += copyLength;
                    bufferedBytes -= copyLength;
                    readBytes += copyLength;
                    if (headOffset == head.Length)
                    {
                        chunks.Dequeue();
                        headOffset = 0;
                    }
                    Monitor.PulseAll(monitor);
                }
            }

            return copied;
        }

        public override long Seek(long offset, SeekOrigin origin)
        {
            lock (monitor)
            {
                ThrowIfDisposed();
                if (origin == SeekOrigin.Current && offset == 0)
                {
                    return readBytes;
                }
            }

            throw new NotSupportedException("Repositioning the live PCM stream is not supported.");
        }

        public override void Flush()
        {
        }

        public override void SetLength(long value)
        {
            throw new NotSupportedException("The live PCM stream has a fixed declared length.");
        }

        public override void Write(byte[] buffer, int offset, int count)
        {
            throw new NotSupportedException("Use WriteChunk from the PCM producer.");
        }

        protected override void Dispose(bool disposing)
        {
            if (disposing)
            {
                lock (monitor)
                {
                    disposed = true;
                    Monitor.PulseAll(monitor);
                }
            }
            base.Dispose(disposing);
        }

        private static void ValidateBufferArguments(byte[] buffer, int offset, int count)
        {
            if (buffer == null)
            {
                throw new ArgumentNullException("buffer");
            }
            if (offset < 0 || count < 0 || offset > buffer.Length - count)
            {
                throw new ArgumentOutOfRangeException();
            }
        }

        private void ThrowIfDisposed()
        {
            if (disposed)
            {
                throw new ObjectDisposedException("ProducerConsumerAudioStream");
            }
        }
    }
}
