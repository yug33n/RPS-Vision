# RPS Vision (Rock Paper Scissors)

**Status:** Work in Progress (Active Development)

**RPS Vision** is a standalone Windows x64 C++23 desktop application for playing real-time Rock-Paper-Scissors against an adaptive AI opponent using your webcam.

## Core Concept
- **Computer Vision:** Local ONNX Runtime hand detection and 21-point 3D landmark tracking with geometric gesture classification.
- **Game AI:** 2nd-order Markov Chain move predictor with persistent match analytics and anti-cheat validation.
- **AI Commentator & Audio:** Hybrid commentary powered by an embedded `llama.cpp` GGUF engine or deterministic template FSM, paired with `miniaudio` SFX and Windows SAPI voice synthesis.
- **Interface:** Hardware-accelerated Dear ImGui + ImPlot dark esports HUD over a threaded OpenCV webcam feed.

## Project Status
Core build scaffolding and dependency manifests are configured on `feat/project-setup`.
Full build instructions and bilingual documentation will be published upon release completion.
See [models/README_MODELS.md](models/README_MODELS.md) for local ONNX and GGUF model setup.
