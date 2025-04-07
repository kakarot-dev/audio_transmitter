import React, { useState, useRef, useEffect } from "react";

const AudioRecorder: React.FC = () => {
  const [recording, setRecording] = useState(false);
  const [audioURL, setAudioURL] = useState<string | null>(null);
  const [isTransmitting, setIsTransmitting] = useState(false);
  const [isConverting, setIsConverting] = useState(false);

  const mediaRecorderRef = useRef<MediaRecorder | null>(null);
  const audioChunksRef = useRef<Blob[]>([]);


  // 🛰️ Check ESP32 /status every 2 seconds
  useEffect(() => {
    const interval = setInterval(async () => {
      try {
        const res = await fetch(`/status`);
        const text = await res.text();
        setIsTransmitting(text.trim() === "transmitting");
        setIsConverting(text.trim() === "converting");
      } catch (err) {
        console.error("❌ Could not reach ESP32:", err);
      }
    }, 1000);

    return () => clearInterval(interval);
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

  const handleSend = async () => {
    if (!audioURL) return;
  
    const blob = await fetch(audioURL).then(res => res.blob());
    const formData = new FormData();
    formData.append("audio", blob, "recorded.mp3");
  
    try {
      const res = await fetch("/upload", {
        method: "POST",
        body: formData,
      });
  
      if (res.ok) {
        alert("✅ MP3 uploaded to ESP32!");
      } else {
        alert("❌ Failed to send MP3.");
      }
    } catch (err) {
      console.error("Upload error:", err);
      alert("❌ Could not reach ESP32.");
    }
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
      <h1>Admin Transmitter</h1>
      <button onClick={recording ? stopRecording : startRecording}>
        {recording ? "⏹️ Stop Recording" : "🎤 Start Recording"}
      </button>

      {audioURL && (
        <>
          <h3>▶️ Recorded Audio</h3>
          <audio src={audioURL} controls />
          <br />

          <button
            onClick={handleSend}
            className="bg-blue-500 text-white px-4 py-2 rounded hover:bg-blue-600 mt-4"
            disabled={isTransmitting || isConverting}
            style={{
              opacity: isTransmitting ? 0.5 : isConverting ? 0.5 : 1,
              cursor: isTransmitting ? "not-allowed" : isConverting ? "not-allowed" : "pointer",
            }}
          >
            {isTransmitting ? "Transmitting..." : isConverting ? "Converting..." : "Send"}
          </button>
        </>
      )}
    </div>
  );
};

export default AudioRecorder;
