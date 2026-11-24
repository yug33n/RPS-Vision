# Models Setup Guide

This directory stores the local AI models used by **RPS Vision**. Large model binaries (`*.onnx`, `*.gguf`) are excluded from Git via `.gitignore` and must be placed here before running computer vision or local LLM commentary.

---

## 1. Required Hand-Tracking Models (ONNX Runtime)

RPS Vision expects two MediaPipe-derived ONNX models from OpenCV Zoo:

| File Name | Model Role | Expected Input | Source |
| :--- | :--- | :--- | :--- |
| `palm_detection.onnx` | Palm bounding-box detector | `192 x 192` RGB/BGR | [opencv/palm_detection_mediapipe](https://huggingface.co/opencv/palm_detection_mediapipe) |
| `hand_landmark.onnx` | 21-keypoint 3D hand landmarker | `224 x 224` RGB/BGR | [opencv/handpose_estimation_mediapipe](https://huggingface.co/opencv/handpose_estimation_mediapipe) |

### Option A: Quick Download via PowerShell

Run from the repository root (`rps-vision/`):

```powershell
$modelsDir = "models"
New-Item -ItemType Directory -Force -Path $modelsDir | Out-Null

Invoke-WebRequest `
  -Uri "https://huggingface.co/opencv/palm_detection_mediapipe/resolve/main/palm_detection_mediapipe_2023feb.onnx" `
  -OutFile "$modelsDir/palm_detection.onnx"

Invoke-WebRequest `
  -Uri "https://huggingface.co/opencv/handpose_estimation_mediapipe/resolve/main/handpose_estimation_mediapipe_2023feb.onnx" `
  -OutFile "$modelsDir/hand_landmark.onnx"
```

### Option B: Verify Shapes or Convert from TFLite (Python)

If you want to inspect the downloaded ONNX input/output tensors locally:

```python
import onnxruntime as ort

for path in ["models/palm_detection.onnx", "models/hand_landmark.onnx"]:
    session = ort.InferenceSession(path, providers=["CPUExecutionProvider"])
    print(f"=== {path} ===")
    for inp in session.get_inputs():
        print(f"  Input : {inp.name} shape={inp.shape} type={inp.type}")
    for out in session.get_outputs():
        print(f"  Output: {out.name} shape={out.shape} type={out.type}")
```

If converting custom MediaPipe `.tflite` graphs yourself, use `tf2onnx`:

```bash
pip install tf2onnx onnxruntime
python -m tf2onnx.convert --tflite palm_detection_full.tflite --output models/palm_detection.onnx --opset 15
python -m tf2onnx.convert --tflite hand_landmark_full.tflite --output models/hand_landmark.onnx --opset 15
```

---

## 2. Optional Local LLM Commentator (`.gguf` for `llama.cpp`)

By default, RPS Vision uses its fast offline **Template Commentator** (`TemplateCommentator`). To enable dynamic neural commentary via embedded `llama.cpp`, place any quantized GGUF instruct model in `models/commentator.gguf` (recommended 4-bit `Q4_K_M`, ~1 GB):

- **Qwen 2.5 1.5B Instruct (Q4_K_M)** — fast, sharp wit, strong English & Russian support.

```powershell
Invoke-WebRequest `
  -Uri "https://huggingface.co/bartowski/Qwen2.5-1.5B-Instruct-GGUF/resolve/main/Qwen2.5-1.5B-Instruct-Q4_K_M.gguf" `
  -OutFile "models/commentator.gguf"
```

When `models/commentator.gguf` is present, you can switch between **Template Engine** and **Local LLM (GGUF)** directly in the right-hand UI panel.
