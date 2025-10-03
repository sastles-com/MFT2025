import pandas as pd
import plotly.graph_objects as go
import layout_visualizer


def test_create_led_layout_figure_returns_scatter3d_per_strip(tmp_path):
    csv_path = tmp_path / "led_layout.csv"
    data = {
        "FaceID": [0, 1, 2, 3, 4],
        "strip": [0, 0, 1, 1, 1],
        "strip_num": [0, 1, 0, 1, 2],
        "x": [0.1, 0.2, -0.1, -0.2, -0.3],
        "y": [0.3, 0.4, 0.5, 0.6, 0.7],
        "z": [0.7, 0.6, 0.5, 0.4, 0.3],
    }
    pd.DataFrame(data).to_csv(csv_path, index=False)

    fig = layout_visualizer.create_led_layout_figure(csv_path)

    assert isinstance(fig, go.Figure)
    assert len(fig.data) == 2

    names = {trace.name for trace in fig.data}
    assert names == {"Strip 0", "Strip 1"}

    for trace in fig.data:
        assert isinstance(trace, go.Scatter3d)
        assert trace.visible is True
        assert trace.legendgroup == trace.name
        assert trace.hovertemplate is not None
        assert "strip" in trace.hovertemplate
        assert "strip_num" in trace.hovertemplate
        assert trace.customdata is not None
        first_custom = trace.customdata[0]
        assert first_custom[0] == trace.meta["strip"]
        assert first_custom[1] in data["strip_num"]


def test_create_led_layout_figure_uses_provided_csv(tmp_path):
    csv_path = tmp_path / "led_layout.csv"
    pd.DataFrame(
        {
            "FaceID": [0],
            "strip": [3],
            "strip_num": [9],
            "x": [0.0],
            "y": [0.0],
            "z": [1.0],
        }
    ).to_csv(csv_path, index=False)

    fig = layout_visualizer.create_led_layout_figure(csv_path)

    assert len(fig.data) == 1
    trace = fig.data[0]
    assert trace.name == "Strip 3"
    assert trace.meta == {"strip": 3}
    assert trace.customdata[0][0] == 3
    assert trace.customdata[0][1] == 9
