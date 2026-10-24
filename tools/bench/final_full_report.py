"""Chart the final bench: stock vs pure-perf vs full, per device, frame generation off and on.

Reads captures/final-full/<device>/results.csv. Also reads results-fgon.csv,
results-fgoff.csv, fg-on/results.csv, and fg-off/results.csv in that folder.
A folder named <device>-fgon or <device>-fgoff is that device with that setting.
A row counts only when valid is 1 (or the valid column is absent and the metric
cell is filled). Blank cells stay blank. This script does not invent a number.

Writes docs/final-bench/report.md and one SVG per metric that has a value.
Stdlib only. No game.

  py -I tools/bench/final_full_report.py
"""
import csv
import os
import sys
from collections import defaultdict

ARMS = (
    ("stock", ("arm1-stocklike", "stock", "stocklike", "stock-like", "arm1", "a1")),
    ("pure-perf", ("arm2-pureperf", "pureperf", "pure-perf", "pure_perf", "arm2", "a2")),
    ("full", ("arm3-fullmod", "fullmod", "full", "arm3", "a3")),
)
ARM_ORDER = [a[0] for a in ARMS]
ARM_COLOR = {"stock": "#8b949e", "pure-perf": "#3fb950", "full": "#58a6ff"}
METRICS = (
    ("avg_fps", "Average fps", ("avg_fps",)),
    ("low1_fps", "1% low fps", ("low1_fps", "low_1_fps")),
    ("hitches", "Hitches", ("hitches", "hitch_count", "hitch", "hitch_n", "hitches_2x")),
    ("cpu_temp", "CPU temp C", ("cpu_temp", "cpu_temp_c", "cpu_c")),
    ("gpu_temp", "GPU temp C", ("gpu_temp", "gpu_temp_c", "gpu_c")),
    ("cpu_power", "CPU power W", ("cpu_power", "cpu_power_w", "cpu_w")),
    ("gpu_power", "GPU power W", ("gpu_power", "gpu_power_w", "gpu_w")),
    ("cpu_util", "CPU util %", ("cpu_util", "cpu_load", "cpu_pct")),
    ("gpu_util", "GPU util %", ("gpu_util", "gpu_load", "gpu_pct")),
)
DEVICE_ORDER = ("pc", "pc2", "ally")
FG_ORDER = ("off", "on")


def repo_root():
    return os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))


def norm_arm(text):
    key = (text or "").strip().lower()
    for name, aliases in ARMS:
        if key in aliases:
            return name
    return None


def norm_fg(text, default):
    key = (text or "").strip().lower()
    if key in ("1", "on", "true", "yes", "fgon", "fg-on"):
        return "on"
    if key in ("0", "off", "false", "no", "fgoff", "fg-off"):
        return "off"
    return default


def norm_device(text):
    key = (text or "").strip().lower().replace(" ", "")
    if key in ("ally", "allyx", "rogally", "rogallyx"):
        return "ally"
    return key


def num(text):
    text = (text or "").strip()
    if not text:
        return None
    try:
        return float(text)
    except ValueError:
        return None


def row_ok(row):
    if "valid" not in row:
        return True
    flag = (row.get("valid") or "").strip().lower()
    return flag in ("1", "true", "yes")


def pick(row, names):
    for name in names:
        if name in row and (row.get(name) or "").strip():
            return num(row[name])
    return None


def load_csv(path, device, default_fg):
    rows = []
    skipped = 0
    with open(path, newline="", encoding="utf-8-sig") as handle:
        for raw in csv.DictReader(handle):
            folded = {(k or "").strip().lower(): (v or "") for k, v in raw.items()}
            if not row_ok(folded):
                skipped += 1
                continue
            arm = norm_arm(folded.get("arm"))
            if not arm:
                skipped += 1
                continue
            item = {
                "device": norm_device(folded.get("device") or device),
                "fg": norm_fg(folded.get("fg") or folded.get("frame_gen") or folded.get("framegen"), default_fg),
                "arm": arm,
                "label": (folded.get("label") or folded.get("run_id") or "").strip(),
                "source": path,
            }
            kept = False
            for key, _label, names in METRICS:
                value = pick(folded, names)
                item[key] = value
                kept = kept or value is not None
            if kept:
                rows.append(item)
            else:
                skipped += 1
    return rows, skipped


def folder_device(name):
    low = name.lower()
    if low.endswith("-fgon"):
        return low[:-5], "on"
    if low.endswith("-fgoff"):
        return low[:-6], "off"
    return low, None


def discover(root):
    base = os.path.join(root, "captures", "final-full")
    found = []
    if not os.path.isdir(base):
        return found
    for name in sorted(os.listdir(base)):
        folder = os.path.join(base, name)
        if not os.path.isdir(folder) or name.lower() == "plan":
            continue
        device, folder_fg = folder_device(name)
        if not device:
            continue
        specs = (
            ("results.csv", folder_fg or "off"),
            ("results-fgoff.csv", "off"),
            ("results-fgon.csv", "on"),
        )
        for filename, fg in specs:
            path = os.path.join(folder, filename)
            if os.path.isfile(path):
                found.append((path, device, fg))
        for fg in FG_ORDER:
            path = os.path.join(folder, "fg-" + fg, "results.csv")
            if os.path.isfile(path):
                found.append((path, device, fg))
    return found


def load_tree(root):
    rows, skipped, sources = [], 0, []
    for path, device, fg in discover(root):
        got, skip = load_csv(path, device, fg)
        rows.extend(got)
        skipped += skip
        sources.append(path)
    return rows, skipped, sources


def mean(values):
    if not values:
        return None
    return sum(values) / len(values)


def summarize(rows):
    buckets = defaultdict(list)
    for row in rows:
        buckets[(row["device"], row["fg"], row["arm"])].append(row)
    cells = []
    for key, group in sorted(buckets.items()):
        cell = {"device": key[0], "fg": key[1], "arm": key[2], "n": len(group)}
        for metric, _label, _names in METRICS:
            cell[metric] = mean([row[metric] for row in group if row[metric] is not None])
        cells.append(cell)
    return cells


def devices_of(cells):
    seen = []
    for name in DEVICE_ORDER:
        if any(c["device"] == name for c in cells):
            seen.append(name)
    for cell in cells:
        if cell["device"] not in seen:
            seen.append(cell["device"])
    return seen or list(DEVICE_ORDER)


def chart_groups(cells):
    devices = devices_of(cells)
    groups = []
    for device in devices:
        for fg in FG_ORDER:
            if any(c["device"] == device and c["fg"] == fg for c in cells):
                groups.append((device, fg))
    return groups


def fg_gap_line(cells):
    if not cells:
        return "Frame generation on is pending. No fg=on row is in these files."
    labels = (("pc", "pc"), ("pc2", "pc2"), ("ally", "the Ally"))
    missing = []
    for device, label in labels:
        has_any = any(c["device"] == device for c in cells)
        has_on = any(c["device"] == device and c["fg"] == "on" for c in cells)
        if has_any and not has_on:
            missing.append(label)
    if not missing:
        return None
    if len(missing) == 1:
        who = missing[0]
    elif len(missing) == 2:
        who = "%s and %s" % (missing[0], missing[1])
    else:
        who = ", ".join(missing[:-1]) + ", and " + missing[-1]
    return "Frame generation on is pending for %s." % who


def fmt(value):
    if value is None:
        return ""
    return "%.2f" % value


def ally_chunk_lines(rows):
    lines = [
        "## Ally chunks",
        "",
        "ROG Ally, Linux/Proton, frame generation off. One row is one fight chunk.",
        "a1 is stock, a2 is pure-perf, a3 is full. The means above include every valid row.",
        "",
        "| chunk | arm | avg_fps | low1_fps | hitches | cpu_temp | gpu_temp | cpu_power | gpu_power | cpu_util | gpu_util |",
        "| " + " | ".join("---" for _ in range(11)) + " |",
    ]
    ally = [row for row in rows if row["device"] == "ally"]
    if not ally:
        lines.append("| | | | | | | | | | | |")
        lines.append("")
        lines.append("No Ally chunk is in captures/final-full/ally.")
        lines.append("")
        return lines
    for row in ally:
        lines.append("| " + " | ".join([
            row.get("label") or "",
            row["arm"],
            fmt(row["avg_fps"]),
            fmt(row["low1_fps"]),
            fmt(row["hitches"]),
            fmt(row["cpu_temp"]),
            fmt(row["gpu_temp"]),
            fmt(row["cpu_power"]),
            fmt(row["gpu_power"]),
            fmt(row["cpu_util"]),
            fmt(row["gpu_util"]),
        ]) + " |")
    lines.append("")
    return lines


def report_md(cells, skipped, sources, rows):
    lines = [
        "# Final bench",
        "",
        "Stock, pure-perf, and full. One mean per device and frame-generation setting.",
        "Only rows with valid=1 and a filled metric are included. Empty cells were not measured.",
        "",
        "Valid groups: %d. Skipped rows: %d." % (len(cells), skipped),
        "",
    ]
    gap = fg_gap_line(cells)
    if gap:
        lines.append(gap)
        lines.append("")
    if not cells:
        lines.append("No valid measurement is in captures/final-full yet. The charts are not drawn.")
        lines.append("")
    header = ["device", "fg", "arm", "n"] + [m[0] for m in METRICS]
    lines.append("| " + " | ".join(header) + " |")
    lines.append("| " + " | ".join("---" for _ in header) + " |")
    if not cells:
        lines.append("| | | | | | | | | | | | | |")
    for cell in cells:
        lines.append("| " + " | ".join(
            [cell["device"], cell["fg"], cell["arm"], str(cell["n"])] + [fmt(cell[m[0]]) for m in METRICS]
        ) + " |")
    lines.append("")
    lines.extend(ally_chunk_lines(rows))
    lines.append("Sources:")
    if sources:
        for path in sources:
            lines.append("- `%s`" % path.replace("\\", "/"))
    else:
        lines.append("- none")
    lines.append("")
    return "\n".join(lines)


def svg_chart(metric, label, cells):
    groups = chart_groups(cells)
    width, height = 920, 320
    left, top, plot_w, plot_h = 48, 36, 760, 220
    vals = [c[metric] for c in cells if c[metric] is not None]
    peak = max(vals) if vals else 1
    if peak <= 0:
        peak = 1
    slot = plot_w / max(len(groups), 1)
    bar_w = min(28, slot / 4)
    parts = [
        '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 %d %d" font-family="Segoe UI,Helvetica,Arial,sans-serif">' % (width, height),
        '<rect width="100%" height="100%" fill="#0d1117"/>',
        '<text x="16" y="22" fill="#e6edf3" font-size="15" font-weight="700">%s</text>' % label,
    ]
    x = 560
    for arm in ARM_ORDER:
        parts.append('<rect x="%d" y="10" width="12" height="12" fill="%s"/>' % (x, ARM_COLOR[arm]))
        parts.append('<text x="%d" y="21" fill="#c9d1d9" font-size="12">%s</text>' % (x + 16, arm))
        x += 110
    for i, (device, fg) in enumerate(groups):
        gx = left + i * slot
        parts.append('<text x="%d" y="%d" fill="#8b949e" font-size="11" text-anchor="middle">%s %s</text>' % (
            gx + slot / 2, top + plot_h + 18, device, "FG " + fg))
        for j, arm in enumerate(ARM_ORDER):
            match = [c for c in cells if c["device"] == device and c["fg"] == fg and c["arm"] == arm]
            value = match[0][metric] if match else None
            if value is None:
                continue
            h = plot_h * (value / peak)
            bx = gx + 8 + j * (bar_w + 4)
            parts.append('<rect x="%.1f" y="%.1f" width="%.1f" height="%.1f" fill="%s"/>' % (
                bx, top + plot_h - h, bar_w, h, ARM_COLOR[arm]))
            parts.append('<text x="%.1f" y="%.1f" fill="#e6edf3" font-size="10" text-anchor="middle">%s</text>' % (
                bx + bar_w / 2, top + plot_h - h - 4, fmt(value)))
    parts.append("</svg>")
    return "\n".join(parts) + "\n"


def png_chart(path, metric, label, cells):
    from PIL import Image, ImageDraw, ImageFont
    groups = chart_groups(cells)
    width, height = 920, 320
    left, top, plot_w, plot_h = 48, 36, 760, 220
    image = Image.new("RGB", (width, height), (13, 17, 23))
    draw = ImageDraw.Draw(image)
    font = ImageFont.load_default()
    vals = [c[metric] for c in cells if c[metric] is not None]
    peak = max(vals) if vals else 1
    if peak <= 0:
        peak = 1
    slot = plot_w / max(len(groups), 1)
    bar_w = min(28, slot / 4)
    colors = {"stock": (139, 148, 158), "pure-perf": (63, 185, 80), "full": (88, 166, 255)}
    draw.text((16, 8), label, fill=(230, 237, 243), font=font)
    x = 560
    for arm in ARM_ORDER:
        draw.rectangle((x, 10, x + 12, 22), fill=colors[arm])
        draw.text((x + 16, 8), arm, fill=(201, 209, 217), font=font)
        x += 110
    for i, (device, fg) in enumerate(groups):
        gx = left + i * slot
        draw.text((gx + 8, top + plot_h + 8), device + " FG " + fg, fill=(139, 148, 158), font=font)
        for j, arm in enumerate(ARM_ORDER):
            match = [c for c in cells if c["device"] == device and c["fg"] == fg and c["arm"] == arm]
            value = match[0][metric] if match else None
            if value is None:
                continue
            h = plot_h * (value / peak)
            bx = gx + 8 + j * (bar_w + 4)
            y0 = top + plot_h - h
            draw.rectangle((bx, y0, bx + bar_w, top + plot_h), fill=colors[arm])
            draw.text((bx, max(top, y0 - 12)), fmt(value), fill=(230, 237, 243), font=font)
    image.save(path, "PNG")


def write_report(root, out_dir=None):
    rows, skipped, sources = load_tree(root)
    cells = summarize(rows)
    out = out_dir or os.path.join(root, "docs", "final-bench")
    os.makedirs(out, exist_ok=True)
    text = report_md(cells, skipped, [os.path.relpath(p, root) for p in sources], rows)
    with open(os.path.join(out, "report.md"), "w", encoding="utf-8", newline="\n") as handle:
        handle.write(text)
    written = []
    for key, label, _names in METRICS:
        if any(c[key] is not None for c in cells):
            name = key + ".svg"
            with open(os.path.join(out, name), "w", encoding="utf-8", newline="\n") as handle:
                handle.write(svg_chart(key, label, cells))
            written.append(name)
            png_name = key + ".png"
            try:
                png_chart(os.path.join(out, png_name), key, label, cells)
            except ImportError:
                continue
            written.append(png_name)
    return {"groups": len(cells), "skipped": skipped, "charts": written, "rows": len(rows)}


def main(argv):
    root = repo_root()
    info = write_report(root)
    print("valid groups %d, valid rows %d, skipped %d, charts %s" % (
        info["groups"], info["rows"], info["skipped"], ", ".join(info["charts"]) or "none"))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
