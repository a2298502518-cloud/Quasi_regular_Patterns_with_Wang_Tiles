"""冻结的全局 QRP 造型试调台；依赖可选历史入口，不代表当前独立瓦片路径。"""

import argparse
import hashlib
import json
import math
import mimetypes
import re
import subprocess
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
MODEL = "direct-qrp-design-v1"
CONTROLS = [
    {"key": "frequency", "label": "疏密", "min": 1.8, "max": 4.8, "step": 0.05, "default": 3.15},
    {"key": "phase_a", "label": "形态 A", "min": -0.6, "max": 0.6, "step": 0.05, "default": 0, "advanced": True},
    {"key": "phase_b", "label": "形态 B", "min": -0.6, "max": 0.6, "step": 0.05, "default": 0, "advanced": True},
    {"key": "angle", "label": "结构旋转", "min": -90, "max": 90, "step": 1, "default": 0, "unit": "°"},
    {"key": "level", "label": "形状扩展", "min": -0.12, "max": 0.12, "step": 0.01, "default": -0.08},
    {"key": "inset", "label": "向内留白", "min": 0, "max": 0.06, "step": 0.005, "default": 0.025},
    {"key": "line_width", "label": "线宽", "min": 0.01, "max": 0.06, "step": 0.002, "default": 0.024},
    {"key": "drawing", "label": "轮廓表达", "choices": [0, 1], "choice_labels": ["面状填充", "几何线描"], "default": 0},
]
DEFAULTS = {control["key"]: control["default"] for control in CONTROLS}
PRESETS = [
    {"name": "几何花簇", "note": "QRP 直接造型 · 面状", "parameters": DEFAULTS},
    {"name": "疏朗单元", "note": "降低密度 · 增加间隔", "parameters": dict(DEFAULTS, frequency=2.4, level=-0.12, inset=0.035)},
    {"name": "细线花簇", "note": "沿同一骨架描线", "parameters": dict(DEFAULTS, drawing=1)},
    {"name": "斜向线描", "note": "旋转与小幅相位变化", "parameters": dict(DEFAULTS, drawing=1, angle=30, phase_a=0.3, phase_b=-0.2)},
]


def validate_parameters(parameters):
    if not isinstance(parameters, dict) or set(parameters) != set(DEFAULTS):
        raise ValueError("参数字段不完整，或含有不支持的字段。")
    result = {}
    for control in CONTROLS:
        key = control["key"]
        value = parameters[key]
        if isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(value):
            raise ValueError(f"{control['label']} 必须是有限数值。")
        if "choices" in control:
            if value not in control["choices"]:
                raise ValueError("请选择已验证的结构候选。")
            value = int(value)
        elif not control["min"] <= value <= control["max"]:
            raise ValueError(f"{control['label']} 超出试调范围。")
        result[key] = value
    return result


class Handler(BaseHTTPRequestHandler):
    def send_data(self, content, mime="application/json; charset=utf-8", status=200):
        self.send_response(status)
        self.send_header("Content-Type", mime)
        self.send_header("Content-Length", str(len(content)))
        self.send_header("X-Content-Type-Options", "nosniff")
        self.end_headers()
        self.wfile.write(content)

    def json_response(self, value, status=200):
        self.send_data(json.dumps(value, ensure_ascii=False).encode("utf-8"), status=status)

    def local_request(self):
        host = self.headers.get("Host", "")
        allowed = {f"127.0.0.1:{self.server.server_port}", f"localhost:{self.server.server_port}"}
        origin = self.headers.get("Origin")
        return host in allowed and (origin is None or origin == f"http://{host}")

    def do_GET(self):
        if not self.local_request():
            self.json_response({"error": "仅允许本机同源访问。"}, 403)
            return
        if self.path == "/":
            self.send_data(Path(__file__).with_name("index.html").read_bytes(), "text/html; charset=utf-8")
        elif self.path == "/api/config":
            self.json_response({"controls": CONTROLS, "presets": PRESETS, "model": MODEL})
        else:
            match = re.fullmatch(r"/artifacts/([0-9a-f]{20})/(render_(?:color|coarse_gray|final_gray|mask)\.png|recipe\.json)", self.path)
            if match:
                path = self.server.cache / match[1] / match[2]
                if path.is_file():
                    self.send_data(path.read_bytes(), mimetypes.guess_type(path)[0] or "application/octet-stream")
                    return
            self.json_response({"error": "文件不存在。"}, 404)

    def do_POST(self):
        if not self.local_request():
            self.json_response({"error": "仅允许本机同源请求。"}, 403)
            return
        if self.path != "/api/render":
            self.json_response({"error": "未知操作。"}, 404)
            return
        try:
            length = int(self.headers.get("Content-Length", 0))
            if not 0 < length <= 4096:
                raise ValueError("请求大小不正确。")
            request = json.loads(self.rfile.read(length))
            parameters = validate_parameters(request.get("parameters"))
            mode = request.get("mode", "preview")
            if mode not in ("preview", "export"):
                raise ValueError("未知的输出模式。")
            pixels = 32 if mode == "preview" else 80
            recipe = {"format": "qrp-recipe", "version": 1, "model": MODEL,
                      "engine_sha256": hashlib.sha256(self.server.executable.read_bytes()).hexdigest(), "parameters": parameters}
            cache_input = json.dumps([recipe, pixels], sort_keys=True, separators=(",", ":"))
            key = hashlib.sha256(cache_input.encode()).hexdigest()[:20]
            directory = self.server.cache / key
            start = time.perf_counter()
            with self.server.render_lock:
                cached = (directory / "recipe.json").is_file()
                if not cached:
                    directory.mkdir(parents=True, exist_ok=True)
                    command = [str(self.server.executable), "--design-render", str(directory), str(pixels)]
                    command.extend(str(parameters[control["key"]]) for control in CONTROLS)
                    completed = subprocess.run(command, capture_output=True, timeout=90,
                        creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
                    if completed.returncode:
                        raise RuntimeError(completed.stderr.decode("utf-8", errors="replace").strip())
                    # 成功标记最后写；失败或中断的结果不会被当作完整缓存。
                    (directory / "recipe.json").write_text(json.dumps(recipe, ensure_ascii=False, indent=2), encoding="utf-8")
            self.json_response({"parameters": parameters, "size": pixels * 20, "cached": cached,
                "seconds": round(time.perf_counter() - start, 3),
                "urls": {view: f"/artifacts/{key}/render_{view}.png" for view in ("color", "coarse_gray", "mask")},
                "recipe": f"/artifacts/{key}/recipe.json"})
        except (ValueError, AttributeError) as error:
            self.json_response({"error": str(error)}, 400)
        except (RuntimeError, subprocess.TimeoutExpired, OSError) as error:
            self.json_response({"error": f"生成失败：{error}"}, 500)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", type=int, default=8766)
    parser.add_argument("--executable", type=Path, default=ROOT / "build-legacy/Release/qrp_legacy_experiment.exe")
    args = parser.parse_args()
    executable = args.executable.resolve()
    if not executable.is_file():
        parser.error(f"请先启用 QRP_BUILD_LEGACY_EXPERIMENTS 并构建 qrp_legacy_experiment：{executable}")
    server = ThreadingHTTPServer(("127.0.0.1", args.port), Handler)
    server.executable = executable
    server.cache = ROOT / "output/qrp-workbench"
    server.render_lock = threading.Lock()
    print(f"QRP workbench: http://127.0.0.1:{args.port}", flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()


if __name__ == "__main__":
    main()
