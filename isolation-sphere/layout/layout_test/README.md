# Layout visualizer

This directory contains a small layout visualizer for the LED sphere used in the project.

Files

- `layout_visualizer.py`: main script to visualize `led_layout.csv` with Plotly.
- `led_layout.csv`: sample layout used by the visualizer.
- `layout_rot.csv`: example output after applying a rotation to a strip.

Usage

Run the visualizer from this directory (it will try a few fallback locations for the CSV):

```sh
uv run layout_visualizer.py [path/to/led_layout.csv]
```

If no path is provided the script defaults to `../scripts/led_layout.csv` and will also search `../data/led_layout.csv` and `layout/layout_test/led_layout.csv`.

Per-point brightness by `strip_num`

The visualizer shades each LED point along its strip according to the `strip_num` column: points with lower `strip_num` appear darker, and higher values appear brighter. This makes per-strip ordering and density easier to inspect.

Rotate a strip and save

The visualizer supports rotating a single strip around an arbitrary axis and saving the resulting layout to CSV.

Example: rotate strip 1 around the Z axis by 45 degrees and save to `led_layout_rot.csv`:

```py
from layout_visualizer import create_led_layout_figure, rotate_and_save_strip

rotate_and_save_strip(csv_path="../scripts/led_layout.csv",
    strip_id=1,
    axis=(0,0,1),
    degrees=45.0,
    out_path="layout_rot.csv")
```

CLI-style usage will be added soon. For now, import the helper function and call it from Python.

Notes

- The script requires `pandas` and `plotly`.
- If you want to generate a PNG headlessly (CI), consider using `plotly.io.write_image` with an installed Kaleido engine.


