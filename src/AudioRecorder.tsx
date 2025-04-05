import React, { useState, useRef, useEffect } from "react";
import { FFmpeg } from "@ffmpeg/ffmpeg";
import { fetchFile, toBlobURL } from "@ffmpeg/util";

const AudioRecorder: React.FC = () => {
  const [recording, setRecording] = useState(false);
  const [audioURL, setAudioURL] = useState<string | null>(null);
  const [convertedURL, setConvertedURL] = useState<string | null>(null);
  const [ffmpegLoaded, setFfmpegLoaded] = useState(false);

  const mediaRecorderRef = useRef<MediaRecorder | null>(null);
  const audioChunksRef = useRef<Blob[]>([]);
  const ffmpegRef = useRef<FFmpeg | null>(null);

  useEffect(() => {
    const loadFFmpeg = async () => {
      const ffmpeg = new FFmpeg();

      try {
        await ffmpeg.load({
          coreURL: await toBlobURL("/ffmpeg/ffmpeg-core.js", "text/javascript"),
          wasmURL: await toBlobURL("/ffmpeg/ffmpeg-core.wasm", "application/wasm"),
          workerURL: await toBlobURL("/ffmpeg/ffmpeg-core.worker.js", "text/javascript"),
        });
        ffmpegRef.current = ffmpeg;
        setFfmpegLoaded(true);
        console.log("FFmpeg loaded successfully");
      } catch (err) {
        console.error("Failed to load FFmpeg:", err);
      }
    };

    loadFFmpeg();
  }, []);

  const startRecording = async () => {
    try {
      const stream = await navigator.mediaDevices.getUserMedia({ audio: true });
      const mediaRecorder = new MediaRecorder(stream);
      mediaRecorderRef.current = mediaRecorder;
      audioChunksRef.current = [];

      mediaRecorder.ondataavailable = (event) => {
        if (event.data.size > 0) {
          audioChunksRef.current.push(event.data);
        }
      };

      mediaRecorder.onstop = () => {
        const blob = new Blob(audioChunksRef.current, { type: "audio/webm" });
        setAudioURL(URL.createObjectURL(blob));
      };

      mediaRecorder.start();
      setRecording(true);
    } catch (err) {
      console.error("Recording failed:", err);
    }
  };

  const stopRecording = () => {
    mediaRecorderRef.current?.stop();
    setRecording(false);
  };

  const convertAudio = async () => {
    if (!ffmpegLoaded || !audioURL || !ffmpegRef.current) {
      alert("FFmpeg not ready or no audio recorded!");
      return;
    }
  
    const response = await fetch(audioURL);
    const blob = await response.blob();
    const ffmpeg = ffmpegRef.current;
  
    await ffmpeg.writeFile("input.webm", await fetchFile(blob));
  
    await ffmpeg.exec([
      "-i", "input.webm",
      "-af", "loudnorm=I=-12:TP=-1.5:LRA=7",
      "-ac", "1",
      "-ar", "16000",
      "-f", "s16le",
      "-acodec", "pcm_s16le",
      "output.raw",
    ]);
  
    const data = await ffmpeg.readFile("output.raw");
    const rawBlob = new Blob([data as ArrayBuffer], {
      type: "application/octet-stream",
    });
  
    setConvertedURL(URL.createObjectURL(rawBlob));
  };
  const handleSend = () => {
    console.log("Send button clicked!");
  };
  return (
    <div
  style={{
    display: "flex",
    flexDirection: "column",
    alignItems: "center",
    textAlign: "center",
    padding: "20px",
    color: "#fff",
    backgroundColor: "#121212",
    borderRadius: "10px",
    boxShadow: "0 0 20px rgba(0, 0, 0, 0.4)",
    width: "90%",
    maxWidth: "500px",
  }}
>
      <h1>🎙️ Audio Recorder</h1>
      <button onClick={recording ? stopRecording : startRecording}>
        {recording ? "⏹️ Stop Recording" : "🎤 Start Recording"}
      </button>

      {audioURL && (
        <>
          <h3>▶️ Recorded Audio</h3>
          <audio src={audioURL} controls />
          <br />
          
          <button onClick={() => console.log("Send button clicked!")} 
          className="bg-blue-500 text-white px-4 py-2 rounded hover:bg-blue-600 mt-4"
       >
  Send
</button>
        </>
      )}

      {convertedURL && (
        <div>
          <h3>📥 Download Converted Audio</h3>
          <a href={convertedURL} download="output.raw">
            Download RAW
          </a>
        </div>
      )}
    </div>
  );
};

export default AudioRecorder;
