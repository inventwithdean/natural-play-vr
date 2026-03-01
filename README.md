# ⚔️ NaturalPlay-VR

**A fully offline Virtual Reality framework where a Vision-Language Model can listen, speak, and explore the world with you.**

### 🎥 (3 minutes demo) [YouTube](https://www.youtube.com/watch?v=FAyTwN2x73Q)
---

## The Vision

Most AI in games use text descriptions of the level and objects for gameplay and cloud AI to perform in game tasks. NaturalPlay-VR is a completely offline, 3D spatial framework that allows an NPC to physically navigate and reason about its environment using pure visual data.

There are no hardcoded navigation maps or pre-written dialogue trees. The NPC literally looks at the world, processes the image through a local VLM (Ministral 8B), and executes spatial commands with minimal latency.

## Key Features

* **Zero-Shot Spatial Reasoning (Set-of-Marks):** ID overlays on objects that are visible only to the VLM. The VLM uses these IDs with tools, proving dynamic object permanence without hardcoded scripts.
* **Shared Visual Context:** Using dual render targets, the player can dynamically hot-swap the VLM's context window. One to let the AI process its own Ego-Vision (for tactical navigation), another to share the Player's Ego-Vision (for dynamic lore generation based on what *you* are looking at).
* **Fully Offline Edge Compute:** Zero cloud latency. Zero API keys. The entire stack - Vision, STT, and TTS runs locally on your hardware.
* **Image Pruning:** To prevent VRAM overflow during extended gameplay, the C++ HTTP bridge automatically prunes the context window, retaining only the latest image view for inference.

---

## The Architecture (PC-VR Compute Split)

To maintain smooth framerates on the Meta Quest 3 while running a heavy 8B VLM, the framework utilizes a PC-VR compute split:

1. **The Client (Quest 3):** Runs the UE5 game instance, piper and whisper models on cpu via `sherpa-onnx`.
2. **The Server (PC Edge):** A local `llama.cpp` server running `Ministral-3-8b` processes the chat completion requests.

<img width="2035" height="962" alt="arch" src="https://github.com/user-attachments/assets/18708752-ac34-4b42-8b37-43b31f369879" />

---

## How to Play

You can download the compiled `.apk` in the **Releases** tab.

> The `.apk` expects a local `llama.cpp` server running the `Ministral-3-8b` model on `192.168.137.1:8080`. This is just the default gateway for most windows devices.

So, You need to download llama-cpp (vulkan build is preferred for compatibility with both amd and nvidia gpus), and launch llama-server with ministral 3 8b. Open the Hostpot in your laptop/PC, connect your VR headset to the hostpot via WiFI. And you're good to go.

The core C++ logic for this bridge can be reviewed in this repository.

---

## ⚖️ Open Source Licenses & Acknowledgments

This framework is built on the shoulders of giants. We would like to acknowledge the following open-source projects and models that made this architecture possible:

| Component | Role in Project | License |
| :--- | :--- | :--- |
| **[llama.cpp](https://github.com/ggml-org/llama.cpp)** | Core inference engine for the VLM backend | MIT |
| **[Ministral 3 8B](https://huggingface.co/mistralai/Ministral-3-8B-Instruct-2512-GGUF)** | Vision-Language Model | Apache 2.0 |
| **[sherpa-onnx](https://github.com/k2-fsa/sherpa-onnx/)** | Onnx inference for TTS and ASR | Apache 2.0 |
| **[Piper Model](https://huggingface.co/rhasspy/piper-voices)** | Text to Speech | MIT |
| **[Whisper Tiny EN](https://huggingface.co/openai/whisper-tiny.en)** | Speech to Text | Apache 2.0 |

## Future Work

* **Fine-Tuning for Spatial Object Permanence:** Training smaller models specifically on 3D Set-of-Marks data to decrease hallucinations and utilizing custom tools (like memory) during training to maintain a spatial map in the context. So Image pruning has minimal effects on the context. For e.g., it can use tools to add the most important things it sees to the context, then the image is pruned.
* **Multi-Agent Swarms:** Expanding the HTTP bridge to handle multiple NPCs.

---
