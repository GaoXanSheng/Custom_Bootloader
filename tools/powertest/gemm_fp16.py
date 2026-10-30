import json
import subprocess
import sys
import threading
import time

import torch


def smi():
    try:
        out = subprocess.check_output(
            ["nvidia-smi", "--query-gpu=clocks.sm,power.draw,temperature.gpu", "--format=csv,noheader,nounits"],
            text=True,
            timeout=2,
        )
        parts = [p.strip() for p in out.strip().split(",")]
        if len(parts) >= 3:
            return float(parts[0]), float(parts[1]), float(parts[2])
    except Exception:
        pass
    return None


def bench(n, iters):
    a = torch.randn(n, n, device="cuda", dtype=torch.float16)
    b = torch.randn(n, n, device="cuda", dtype=torch.float16)
    for _ in range(5):
        c = a @ b
    torch.cuda.synchronize()

    samples = []
    stop = False

    def sampler():
        while not stop:
            s = smi()
            if s:
                samples.append(s)
            time.sleep(0.2)

    t = threading.Thread(target=sampler)
    t.start()
    t0 = time.perf_counter()
    for _ in range(iters):
        c = a @ b
    torch.cuda.synchronize()
    elapsed = time.perf_counter() - t0
    stop = True
    t.join()

    tflops = 2.0 * n ** 3 * iters / elapsed / 1e12
    clocks = [s[0] for s in samples]
    powers = [s[1] for s in samples]
    temps = [s[2] for s in samples]
    return {
        "n": n,
        "iters": iters,
        "tflops": round(tflops, 2),
        "ms_per_iter": round(elapsed / iters * 1000, 2),
        "clock_max_mhz": int(max(clocks)) if clocks else 0,
        "power_max_w": round(max(powers), 1) if powers else 0.0,
        "temp_max_c": round(max(temps), 1) if temps else 0.0,
    }


def main():
    if not torch.cuda.is_available():
        print("CUDA not available")
        return 1
    prop = torch.cuda.get_device_properties(0)
    print("GPU: %s (%d SMs, %.1f GB)" % (prop.name, prop.multi_processor_count, prop.total_memory / 1e9))
    print("torch %s, cuda %s, fp16_reduced_precision_reduction=%s" % (
        torch.__version__,
        torch.version.cuda,
        torch.backends.cuda.matmul.allow_fp16_reduced_precision_reduction,
    ))

    results = []
    for n, iters in ((4096, 200), (8192, 60)):
        r = bench(n, iters)
        results.append(r)
        print("FP16 GEMM %d: %.2f TFLOPS | %.2f ms/iter | SM max %d MHz | power max %.1f W | temp max %.1f C" % (
            r["n"], r["tflops"], r["ms_per_iter"], r["clock_max_mhz"], r["power_max_w"], r["temp_max_c"],
        ))

    print("JSON " + json.dumps(results, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    sys.exit(main())
