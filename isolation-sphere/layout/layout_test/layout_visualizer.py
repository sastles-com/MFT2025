from __future__ import annotations

from pathlib import Path
from typing import Iterable, Union

import pandas as pd
import plotly.graph_objects as go
from plotly.colors import qualitative

CsvPath = Union[str, Path]


def _resolve_csv_path(csv_path: CsvPath) -> Path:
    path = Path(csv_path)
    if not path.is_absolute():
        path = (Path(__file__).parent / path).resolve()
    if path.exists():
        return path

    # Fallback: try a few common repository locations (scripts/, data/, layout/layout_test/)
    repo_root = Path(__file__).resolve().parents[2]
    candidates = [
        repo_root / "scripts" / Path(csv_path).name,
        repo_root / "data" / Path(csv_path).name,
        repo_root / "layout" / "layout_test" / Path(csv_path).name,
        repo_root / Path(csv_path),
    ]
    for cand in candidates:
        if cand.exists():
            return cand

    tried = [str(path)] + [str(p) for p in candidates]
    raise FileNotFoundError(f"LED layout CSV not found. Tried: {tried}")


def _color_cycle() -> Iterable[str]:
    palette = qualitative.Safe + qualitative.Dark24 + qualitative.Light24
    while True:
        for color in palette:
            yield color


def _parse_color_to_rgb(color_str: str) -> tuple[int, int, int]:
    """Parse a plotly color string (hex or rgb(...)) to an (r,g,b) tuple."""
    s = color_str.strip()
    if s.startswith("rgb"):
        # rgb(12,34,56)
        inside = s[s.find("(") + 1:s.find(")")]
        parts = [int(p.strip()) for p in inside.split(",")]
        return (parts[0], parts[1], parts[2])
    if s.startswith("#"):
        hexs = s.lstrip("#")
        if len(hexs) == 3:
            r = int(hexs[0] * 2, 16)
            g = int(hexs[1] * 2, 16)
            b = int(hexs[2] * 2, 16)
        else:
            r = int(hexs[0:2], 16)
            g = int(hexs[2:4], 16)
            b = int(hexs[4:6], 16)
        return (r, g, b)
    # fallback: try to parse as comma-separated
    try:
        parts = [int(p.strip()) for p in s.split(",")]
        return (parts[0], parts[1], parts[2])
    except Exception:
        return (128, 128, 128)


def _scale_rgb(rgb: tuple[int, int, int], factor: float) -> tuple[int, int, int]:
    r = max(0, min(255, int(rgb[0] * factor)))
    g = max(0, min(255, int(rgb[1] * factor)))
    b = max(0, min(255, int(rgb[2] * factor)))
    return (r, g, b)


def _rotate_point(pt: tuple[float, float, float], axis: tuple[float, float, float], angle_deg: float) -> tuple[float, float, float]:
    """Rotate point pt around axis (x,y,z) by angle_deg degrees using Rodrigues' formula."""
    import math

    x, y, z = pt
    ax, ay, az = axis
    # normalize axis
    norm = math.sqrt(ax * ax + ay * ay + az * az)
    if norm == 0:
        return pt
    ux, uy, uz = ax / norm, ay / norm, az / norm

    theta = math.radians(angle_deg)
    cos_t = math.cos(theta)
    sin_t = math.sin(theta)

    # Rodrigues rotation
    # v_rot = v*cos + (k x v)*sin + k*(k.v)*(1-cos)
    # cross product k x v
    cx = uy * z - uz * y
    cy = uz * x - ux * z
    cz = ux * y - uy * x

    dot = ux * x + uy * y + uz * z

    rx = x * cos_t + cx * sin_t + ux * dot * (1 - cos_t)
    ry = y * cos_t + cy * sin_t + uy * dot * (1 - cos_t)
    rz = z * cos_t + cz * sin_t + uz * dot * (1 - cos_t)

    return (float(rx), float(ry), float(rz))


def create_led_layout_figure(csv_path: CsvPath) -> go.Figure:
    resolved_path = _resolve_csv_path(csv_path)
    data = pd.read_csv(resolved_path)

    required_columns = {"strip", "strip_num", "x", "y", "z"}
    missing = required_columns - set(data.columns)
    if missing:
        missing_list = ", ".join(sorted(missing))
        raise ValueError(f"CSV is missing required columns: {missing_list}")

    traces = []
    color_iter = _color_cycle()

    for strip_value, strip_df in data.groupby("strip"):
        strip_id = int(strip_value)
        color = next(color_iter)

        customdata = list(zip(strip_df["strip"], strip_df["strip_num"]))

        # compute per-point color scaling by strip_num (0..max -> dark->bright)
        max_strip_num = int(strip_df["strip_num"].max()) if len(strip_df) > 0 else 1
        base_rgb = _parse_color_to_rgb(color)
        colors_rgb = []
        for sn in strip_df["strip_num"]:
            if max_strip_num <= 0:
                factor = 1.0
            else:
                # map 0..max -> factor 0.3 .. 1.0 (dark to bright)
                factor = 0.3 + 0.7 * (float(sn) / float(max_strip_num))
            r, g, b = _scale_rgb(base_rgb, factor)
            colors_rgb.append(f"rgb({r},{g},{b})")

        trace = go.Scatter3d(
            x=strip_df["x"],
            y=strip_df["y"],
            z=strip_df["z"],
            mode="markers",
            marker=dict(size=4, color=colors_rgb, opacity=0.85),
            name=f"Strip {strip_id}",
            legendgroup=f"Strip {strip_id}",
            customdata=customdata,
            hovertemplate=(
                "strip %{customdata[0]}<br>"
                "strip_num %{customdata[1]}<br>"
                "x %{x:.3f}<br>"
                "y %{y:.3f}<br>"
                "z %{z:.3f}<extra></extra>"
            ),
            visible=True,
            meta={"strip": strip_id},
        )
        traces.append(trace)

    fig = go.Figure(data=traces)
    fig.update_layout(
        title="LED Layout by Strip",
        scene=dict(
            xaxis_title="X",
            yaxis_title="Y",
            zaxis_title="Z",
            aspectmode="data",
        ),
        legend=dict(title="Strips"),
    )

    return fig


def main(argv: Iterable[str] | None = None) -> None:
    import argparse

    parser = argparse.ArgumentParser(description="LED layout visualizer")
    parser.add_argument(
        "csv_path",
        nargs="?",
        default=Path("led_layout.csv"),
        help="Path to led_layout.csv",
    )
    parser.add_argument("--rotate-strip", type=int, default=None,
                        help="Strip id to rotate (integer)")
    parser.add_argument("--axis", type=str, default="0,0,1",
                        help="Rotation axis as comma-separated x,y,z (default 0,0,1)")
    parser.add_argument("--angle", type=float, default=0.0,
                        help="Rotation angle in degrees (positive = right-hand rule)")
    parser.add_argument("--out", type=str, default=None,
                        help="Output CSV path to save rotated layout (optional)")
    parser.add_argument("--no-show", action="store_true",
                        help="Do not open interactive figure (useful for headless runs)")
    args = parser.parse_args(list(argv) if argv is not None else None)

    # If rotation requested, load CSV, apply rotation and optionally save
    if args.rotate_strip is not None and abs(args.angle) > 1e-9:
        # load
        df_path = _resolve_csv_path(args.csv_path)
        data = pd.read_csv(df_path)

        # parse axis
        axis_parts = [float(p) for p in args.axis.split(",")]
        if len(axis_parts) != 3:
            raise ValueError("--axis must be three comma-separated numbers e.g. 0,0,1")
        axis = tuple(axis_parts)

        # apply rotation to rows with matching strip id
        def _rotate_row(row):
            if int(row["strip"]) != int(args.rotate_strip):
                return row
            x, y, z = float(row["x"]), float(row["y"]), float(row["z"])
            rx, ry, rz = _rotate_point((x, y, z), axis, args.angle)
            row["x"] = rx
            row["y"] = ry
            row["z"] = rz
            return row

        data = data.apply(_rotate_row, axis=1)

        # save if requested
        if args.out:
            out_path = Path(args.out)
        else:
            # default: sibling scripts/led_layout_rot.csv
            out_path = (Path(__file__).resolve().parents[2] / "scripts" / "led_layout_rot.csv")
        data.to_csv(out_path, index=False)
        print(f"Saved rotated layout to: {out_path}")

        # visualize the rotated data
        fig = create_led_layout_figure(out_path)
    else:
        fig = create_led_layout_figure(args.csv_path)

    if not args.no_show:
        fig.show()


if __name__ == "__main__":
    main()
