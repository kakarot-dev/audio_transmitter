const express = require("express");
const multer = require("multer");
const fs = require("fs");
const { execFile } = require("child_process");
const ffmpegPath = require("ffmpeg-static");

const app = express();
const upload = multer({ dest: "uploads/" });

app.post("/convert", upload.single("audio"), (req, res) => {
  const inputPath = req.file.path;
  const outputPath = `uploads/${req.file.filename}.raw`;

  execFile(ffmpegPath, [
    "-i", inputPath,
    "-af", "loudnorm=I=-12:TP=-1.5:LRA=7",
    "-ac", "1",
    "-ar", "16000",
    "-f", "s16le",
    "-acodec", "pcm_s16le",
    outputPath,
  ], (err) => {
    if (err) {
      console.error(err);
      return res.status(500).send("Conversion failed");
    }

    res.download(outputPath, "output.raw", () => {
      fs.unlinkSync(inputPath);
      fs.unlinkSync(outputPath);
    });
  });
});

app.get("/", (_, res) => {
  res.send("FFmpeg API is live!");
});

app.listen(3000, () => console.log("Listening on port 3000"));
