# ⚔️ NaturalPlay-VR

**A fully offline Virtual Reality framework where a Vision-Language Model can listen, speak, and explore the world with you.**

### 🎥 [link to youtube]


### [architecture diagram]

---

## The Vision

Most AI in games use text descriptions of the level and objects for gameplay and cloud AI to perform in game tasks. NaturalPlay-VR is a completely offline, 3D spatial framework that allows an NPC to physically navigate and reason about its environment using pure visual data.

There are no hardcoded navigation maps or pre-written dialogue trees. The NPC literally looks at the world, processes the image through a local VLM (Ministral 8B), and executes spatial commands with minimal latency.

## Key Features

* **Zero-Shot Spatial Reasoning (Set-of-Marks):** ID overlays on objects that are visible only to the VLM. The VLM uses these IDs with tools, proving dynamic object permanence without hardcoded scripts.
* **Shared Visual Context:** Using dual render targets, the player can dynamically hot-swap the VLM's context window. One to let the AI process its own Ego-Vision (for tactical navigation), another to share the Player's Ego-Vision (for dynamic lore generation based on what *you* are looking at).
* **Fully Offline Edge Compute:** Zero cloud latency. Zero API keys. The entire stack - Vision, STT, and TTS runs locally on your hardware.
* **Intelligent Image Pruning:** To prevent VRAM overflow during extended gameplay, the C++ HTTP bridge automatically prunes the context window, retaining only the most relevant, recent frames for inference.

---

## The Architecture (PC-VR Compute Split)

To maintain smooth framerates on the Meta Quest 3 while running a heavy 8B VLM, the framework utilizes a PC-VR compute split:

1. **The Client (Quest 3):** Runs the UE5 game instance, captures 2D render targets of the environment, converts them to Base64, and captures player voice audio via `sherpa-onnx` (Whisper).
2. **The Server (PC Edge):** A local `llama.cpp` server running `Ministral-3-8b` processes the Base64 frames and user prompts, returning response.
3. **The Execution:** The C++ HTTP Bridge parses the JSON if needed, triggering the Spatial Registry to map the chosen ID back to a physical 3D actor, executing the movement or triggering the `Piper` TTS subsystem on normal responses.

---

## How to Play

You can download the compiled `.apk` in the **Releases** tab.

> Because this game relies on local, privacy-first Edge AI, the `.apk` expects a local `llama.cpp` server running the `Ministral-3-8b` model on `192.168.137.1:8080`. 

So, You need to download llama-cpp (vulkan build is preferred for compatibility with both amd and nvidia gpus), and run llama-server. Open the Hostpot in your laptop/PC, connect your VR headset to the hostpot via WiFI. And you're good to go.

The core C++ logic for this bridge can be reviewed in this repository.

---

## Future Work

* **Fine-Tuning for Spatial Object Permanence:** Training smaller models specifically on 3D Set-of-Marks data to decrease hallucinations and utilizing custom tools (like memory) during training to maintain a spatial map in the context. So Image pruning has minimal effects on the context.
* **Multi-Agent Swarms:** Expanding the HTTP bridge to handle multiple NPCs.

---