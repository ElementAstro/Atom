import os
import json
import tempfile
import pytest
from unittest.mock import patch, MagicMock, mock_open
from io import StringIO
import sys
from matplotlib.figure import Figure

# filepath: atom/tests/test_charts.py

# Import functions from the module we're testing
from atom.tests.charts import (
    load_data,
    validate_metric,
    get_available_metrics,
    set_style,
    generate_bar_chart,
    generate_line_chart,
    generate_scatter_chart,
    generate_pie_chart,
    generate_histogram,
    generate_heatmap,
    generate_all_charts,
    generate_report,
    ChartGenerator,
    plot_from_json,
    main
)


@pytest.fixture
def sample_data():
    """Create sample data for testing."""
    return {
        "suite1": [
            {"metric1": 10, "metric2": 5, "metric3": 7},
            {"metric1": 12, "metric2": 6, "metric3": 8},
            {"metric1": 11, "metric2": 4, "metric3": 9}
        ],
        "suite2": [
            {"metric1": 8, "metric2": 7, "metric3": 5},
            {"metric1": 9, "metric2": 8, "metric3": 6},
            {"metric1": 7, "metric2": 6, "metric3": 4}
        ]
    }


@pytest.fixture
def json_file(sample_data):
    """Create a temporary JSON file with sample data."""
    with tempfile.NamedTemporaryFile(suffix='.json', delete=False, mode='w') as f:
        json.dump(sample_data, f)
        filename = f.name
    yield filename
    os.remove(filename)


@pytest.fixture
def output_dir():
    """Create a temporary directory for output files."""
    with tempfile.TemporaryDirectory() as tmpdirname:
        yield tmpdirname


# Test load_data function
def test_load_data_success(json_file):
    data = load_data(json_file)
    assert "suite1" in data
    assert "suite2" in data
    assert len(data["suite1"]) == 3
    assert data["suite1"][0]["metric1"] == 10


def test_load_data_file_not_found():
    with pytest.raises(SystemExit) as pytest_wrapped_e:
        load_data("nonexistent_file.json")
    assert pytest_wrapped_e.type == SystemExit
    assert pytest_wrapped_e.value.code == 1


def test_load_data_invalid_json():
    m = mock_open(read_data="{invalid json")
    with patch("builtins.open", m):
        with pytest.raises(SystemExit) as pytest_wrapped_e:
            load_data("invalid.json")
        assert pytest_wrapped_e.type == SystemExit
        assert pytest_wrapped_e.value.code == 1


# Test validate_metric function
def test_validate_metric_valid(sample_data):
    # Should not raise an exception
    validate_metric(sample_data, "metric1")
    validate_metric(sample_data, "metric2")
    validate_metric(sample_data, "metric3")


def test_validate_metric_invalid(sample_data):
    with pytest.raises(ValueError) as e:
        validate_metric(sample_data, "nonexistent_metric")
    assert "not found in data" in str(e.value)


# Test get_available_metrics function
def test_get_available_metrics(sample_data):
    metrics = get_available_metrics(sample_data)
    assert "metric1" in metrics
    assert "metric2" in metrics
    assert "metric3" in metrics
    assert len(metrics) == 3


def test_get_available_metrics_empty_data():
    empty_data = {}
    metrics = get_available_metrics(empty_data)
    assert metrics == []


# Test set_style function
@patch("matplotlib.pyplot.style.use")
def test_set_style_default(mock_style_use):
    set_style()
    mock_style_use.assert_called_once_with('default')


@patch("matplotlib.pyplot.style.use")
def test_set_style_dark_mode(mock_style_use):
    set_style(dark_mode=True)
    mock_style_use.assert_called_once_with('dark_background')


@patch("matplotlib.pyplot.style.use")
def test_set_style_seaborn(mock_style_use):
    set_style(style="seaborn")
    mock_style_use.assert_not_called()  # Because sns.set_theme() is called instead


@patch("seaborn.set_theme")
def test_set_style_seaborn_theme(mock_set_theme):
    set_style(style="seaborn")
    mock_set_theme.assert_called_once()


# Test chart generation functions with mocked plt to avoid actual rendering
@patch("matplotlib.pyplot.figure", return_value=MagicMock())
@patch("matplotlib.pyplot.savefig")
@patch("matplotlib.pyplot.close")
@patch("os.makedirs")
def test_generate_bar_chart(mock_makedirs, mock_close, mock_savefig, mock_figure, sample_data, output_dir):
    output_file = os.path.join(output_dir, "test_bar.png")
    generate_bar_chart(sample_data, "metric1", output_file)

    mock_makedirs.assert_called_once()
    mock_savefig.assert_called_once_with(output_file, dpi=300)
    mock_close.assert_called_once()


@patch("matplotlib.pyplot.figure", return_value=MagicMock())
@patch("matplotlib.pyplot.savefig")
@patch("matplotlib.pyplot.close")
@patch("os.makedirs")
def test_generate_line_chart(mock_makedirs, mock_close, mock_savefig, mock_figure, sample_data, output_dir):
    output_file = os.path.join(output_dir, "test_line.png")
    generate_line_chart(sample_data, "metric1", output_file)

    mock_makedirs.assert_called_once()
    mock_savefig.assert_called_once_with(output_file, dpi=300)
    mock_close.assert_called_once()


@patch("matplotlib.pyplot.figure", return_value=MagicMock())
@patch("matplotlib.pyplot.savefig")
@patch("matplotlib.pyplot.close")
@patch("os.makedirs")
def test_generate_scatter_chart(mock_makedirs, mock_close, mock_savefig, mock_figure, sample_data, output_dir):
    output_file = os.path.join(output_dir, "test_scatter.png")
    generate_scatter_chart(sample_data, "metric1", "metric2", output_file)

    mock_makedirs.assert_called_once()
    mock_savefig.assert_called_once_with(output_file, dpi=300)
    mock_close.assert_called_once()


@patch("matplotlib.pyplot.figure", return_value=MagicMock())
@patch("matplotlib.pyplot.savefig")
@patch("matplotlib.pyplot.close")
@patch("os.makedirs")
def test_generate_pie_chart(mock_makedirs, mock_close, mock_savefig, mock_figure, sample_data, output_dir):
    output_file = os.path.join(output_dir, "test_pie.png")
    generate_pie_chart(sample_data, "metric1", output_file)

    mock_makedirs.assert_called_once()
    mock_savefig.assert_called_once_with(output_file, dpi=300)
    mock_close.assert_called_once()


@patch("matplotlib.pyplot.figure", return_value=MagicMock())
@patch("matplotlib.pyplot.savefig")
@patch("matplotlib.pyplot.close")
@patch("os.makedirs")
@patch("seaborn.histplot")
def test_generate_histogram(mock_histplot, mock_makedirs, mock_close, mock_savefig, mock_figure, sample_data, output_dir):
    output_file = os.path.join(output_dir, "test_histogram.png")
    generate_histogram(sample_data, "metric1", output_file)

    mock_makedirs.assert_called_once()
    mock_savefig.assert_called_once_with(output_file, dpi=300)
    mock_close.assert_called_once()
    mock_histplot.assert_called_once()


@patch("matplotlib.pyplot.figure", return_value=MagicMock())
@patch("matplotlib.pyplot.savefig")
@patch("matplotlib.pyplot.close")
@patch("os.makedirs")
@patch("seaborn.heatmap")
def test_generate_heatmap(mock_heatmap, mock_makedirs, mock_close, mock_savefig, mock_figure, sample_data, output_dir):
    output_file = os.path.join(output_dir, "test_heatmap.png")
    generate_heatmap(sample_data, ["metric1", "metric2"], output_file)

    mock_makedirs.assert_called_once()
    mock_savefig.assert_called_once_with(output_file, dpi=300)
    mock_close.assert_called_once()
    mock_heatmap.assert_called_once()


# Test generate_all_charts function
@patch("atom.tests.charts.generate_bar_chart")
@patch("atom.tests.charts.generate_line_chart")
@patch("atom.tests.charts.generate_pie_chart")
@patch("atom.tests.charts.generate_histogram")
@patch("atom.tests.charts.generate_scatter_chart")
@patch("atom.tests.charts.generate_heatmap")
@patch("os.makedirs")
def test_generate_all_charts(mock_makedirs, mock_heatmap, mock_scatter, mock_histogram,
                             mock_pie, mock_line, mock_bar, sample_data, output_dir):
    metrics = ["metric1", "metric2", "metric3"]
    generate_all_charts(sample_data, metrics, output_dir)

    assert mock_bar.call_count == 3
    assert mock_line.call_count == 3
    assert mock_pie.call_count == 3
    assert mock_histogram.call_count == 3
    # 3 metrics, so 3 combinations: (m1,m2), (m1,m3), (m2,m3)
    assert mock_scatter.call_count == 3
    assert mock_heatmap.call_count == 1
    mock_makedirs.assert_called_once_with(output_dir, exist_ok=True)


# Test generate_report function
@patch("atom.tests.charts.generate_all_charts")
@patch("builtins.open", new_callable=mock_open)
@patch("os.makedirs")
def test_generate_report(mock_makedirs, mock_file_open, mock_gen_all_charts, sample_data, output_dir):
    metrics = ["metric1", "metric2"]
    report_path = generate_report(sample_data, metrics, output_dir)

    assert report_path == os.path.join(output_dir, "report.html")
    mock_makedirs.assert_called()
    mock_gen_all_charts.assert_called_once()
    mock_file_open.assert_called()


# Test ChartGenerator class
@patch("atom.tests.charts.load_data")
def test_chart_generator_init_with_json(mock_load_data, sample_data):
    mock_load_data.return_value = sample_data
    generator = ChartGenerator(json_file="test.json")
    mock_load_data.assert_called_once_with("test.json")
    assert generator.metrics == list(sample_data["suite1"][0].keys())


def test_chart_generator_init_with_data(sample_data):
    generator = ChartGenerator(data=sample_data)
    assert generator.data == sample_data
    assert generator.style == "default"
    assert not generator.dark_mode


def test_chart_generator_init_no_data_no_file():
    with pytest.raises(ValueError) as e:
        ChartGenerator()
    assert "Either data or json_file must be provided" in str(e.value)


@patch("atom.tests.charts.generate_bar_chart")
def test_chart_generator_bar_chart(mock_bar_chart, sample_data):
    generator = ChartGenerator(data=sample_data)
    output_file = generator.bar_chart("metric1")
    mock_bar_chart.assert_called_once()
    assert output_file == "metric1_bar.png"


@patch("atom.tests.charts.generate_line_chart")
def test_chart_generator_line_chart(mock_line_chart, sample_data):
    generator = ChartGenerator(data=sample_data)
    output_file = generator.line_chart("metric1")
    mock_line_chart.assert_called_once()
    assert output_file == "metric1_line.png"


@patch("atom.tests.charts.generate_scatter_chart")
def test_chart_generator_scatter_chart(mock_scatter_chart, sample_data):
    generator = ChartGenerator(data=sample_data)
    output_file = generator.scatter_chart("metric1", "metric2")
    mock_scatter_chart.assert_called_once()
    assert output_file == "metric2_vs_metric1_scatter.png"


@patch("atom.tests.charts.generate_pie_chart")
def test_chart_generator_pie_chart(mock_pie_chart, sample_data):
    generator = ChartGenerator(data=sample_data)
    output_file = generator.pie_chart("metric1")
    mock_pie_chart.assert_called_once()
    assert output_file == "metric1_pie.png"


@patch("atom.tests.charts.generate_histogram")
def test_chart_generator_histogram(mock_histogram, sample_data):
    generator = ChartGenerator(data=sample_data)
    output_file = generator.histogram("metric1")
    mock_histogram.assert_called_once()
    assert output_file == "metric1_histogram.png"


@patch("atom.tests.charts.generate_heatmap")
def test_chart_generator_heatmap(mock_heatmap, sample_data):
    generator = ChartGenerator(data=sample_data)
    output_file = generator.heatmap()
    mock_heatmap.assert_called_once()
    assert output_file == "metrics_heatmap.png"


@patch("atom.tests.charts.generate_all_charts")
def test_chart_generator_all_charts(mock_all_charts, sample_data):
    generator = ChartGenerator(data=sample_data)
    out_dir = generator.all_charts()
    mock_all_charts.assert_called_once()
    assert out_dir == "charts"


@patch("atom.tests.charts.generate_report")
def test_chart_generator_generate_report(mock_gen_report, sample_data):
    generator = ChartGenerator(data=sample_data)
    generator.generate_report()
    mock_gen_report.assert_called_once()


# Test plot_from_json function
@patch("atom.tests.charts.load_data")
@patch("atom.tests.charts.get_available_metrics")
@patch("atom.tests.charts.generate_all_charts")
def test_plot_from_json(mock_gen_all_charts, mock_get_metrics, mock_load_data, sample_data):
    mock_load_data.return_value = sample_data
    mock_get_metrics.return_value = ["metric1", "metric2", "metric3"]

    plot_from_json("test.json")

    mock_load_data.assert_called_once_with("test.json")
    mock_get_metrics.assert_called_once()
    mock_gen_all_charts.assert_called_once()


# Test main function and command line argument handling
@patch("sys.argv", ["charts.py", "test.json"])
@patch("atom.tests.charts.load_data")
@patch("atom.tests.charts.get_available_metrics")
@patch("atom.tests.charts.generate_bar_chart")
@patch("atom.tests.charts.generate_line_chart")
@patch("atom.tests.charts.generate_pie_chart")
@patch("atom.tests.charts.generate_histogram")
@patch("atom.tests.charts.generate_heatmap")
@patch("os.makedirs")
def test_main_default_args(mock_makedirs, mock_heatmap, mock_histogram, mock_pie, mock_line,
                           mock_bar, mock_get_metrics, mock_load_data, sample_data):
    mock_load_data.return_value = sample_data
    mock_get_metrics.return_value = ["metric1", "metric2", "metric3"]

    with patch.object(sys, 'argv', ["charts.py", "test.json"]):
        main()

    mock_load_data.assert_called_once_with("test.json")
    mock_get_metrics.assert_called_once()
    assert mock_bar.call_count == 3
    assert mock_line.call_count == 3
    assert mock_pie.call_count == 3
    assert mock_histogram.call_count == 3
    mock_heatmap.assert_called_once()


@patch("sys.argv", ["charts.py", "test.json", "--chart-type", "bar"])
@patch("atom.tests.charts.load_data")
@patch("atom.tests.charts.get_available_metrics")
@patch("atom.tests.charts.generate_bar_chart")
@patch("os.makedirs")
def test_main_bar_chart_only(mock_makedirs, mock_bar, mock_get_metrics, mock_load_data, sample_data):
    mock_load_data.return_value = sample_data
    mock_get_metrics.return_value = ["metric1", "metric2", "metric3"]

    with patch.object(sys, 'argv', ["charts.py", "test.json", "--chart-type", "bar"]):
        main()

    assert mock_bar.call_count == 3  # One for each metric


@patch("sys.argv", ["charts.py", "test.json", "--metrics", "metric1", "metric2"])
@patch("atom.tests.charts.load_data")
@patch("atom.tests.charts.generate_bar_chart")
@patch("atom.tests.charts.generate_line_chart")
@patch("atom.tests.charts.generate_scatter_chart")
@patch("atom.tests.charts.generate_pie_chart")
@patch("atom.tests.charts.generate_histogram")
@patch("atom.tests.charts.generate_heatmap")
@patch("os.makedirs")
def test_main_specific_metrics(mock_makedirs, mock_heatmap, mock_histogram, mock_pie,
                               mock_scatter, mock_line, mock_bar, mock_load_data, sample_data):
    mock_load_data.return_value = sample_data

    with patch.object(sys, 'argv', ["charts.py", "test.json", "--metrics", "metric1", "metric2"]):
        main()

    assert mock_bar.call_count == 2
    assert mock_line.call_count == 2
    assert mock_pie.call_count == 2
    assert mock_histogram.call_count == 2
    mock_heatmap.assert_called_once()


@patch("sys.argv", ["charts.py", "test.json", "--report"])
@patch("atom.tests.charts.load_data")
@patch("atom.tests.charts.get_available_metrics")
@patch("atom.tests.charts.generate_report")
def test_main_generate_report(mock_gen_report, mock_get_metrics, mock_load_data, sample_data):
    mock_load_data.return_value = sample_data
    mock_get_metrics.return_value = ["metric1", "metric2", "metric3"]
    mock_gen_report.return_value = "/path/to/report.html"

    with patch.object(sys, 'argv', ["charts.py", "test.json", "--report"]):
        with patch('sys.stdout', new=StringIO()) as fake_out:
            main()
            assert "Report generated" in fake_out.getvalue()

    mock_gen_report.assert_called_once()


@patch("sys.argv", ["charts.py", "test.json", "--list-metrics"])
@patch("atom.tests.charts.load_data")
@patch("atom.tests.charts.get_available_metrics")
def test_main_list_metrics(mock_get_metrics, mock_load_data, sample_data):
    mock_load_data.return_value = sample_data
    mock_get_metrics.return_value = ["metric1", "metric2", "metric3"]

    with patch.object(sys, 'argv', ["charts.py", "test.json", "--list-metrics"]):
        with patch('sys.stdout', new=StringIO()) as fake_out:
            main()
            output = fake_out.getvalue()
            assert "Available metrics:" in output
            assert "metric1" in output
            assert "metric2" in output
            assert "metric3" in output

    mock_get_metrics.assert_called_once()


@patch("sys.argv", ["charts.py", "test.json", "--scatter-metrics", "metric1", "metric2"])
@patch("atom.tests.charts.load_data")
@patch("atom.tests.charts.generate_scatter_chart")
@patch("os.makedirs")
def test_main_specific_scatter_metrics(mock_makedirs, mock_scatter, mock_load_data, sample_data):
    mock_load_data.return_value = sample_data

    with patch.object(sys, 'argv', ["charts.py", "test.json", "--scatter-metrics", "metric1", "metric2"]):
        main()

    mock_scatter.assert_called_once()
    args = mock_scatter.call_args[0]
    assert args[1] == "metric1"  # metric_x
    assert args[2] == "metric2"  # metric_y


if __name__ == "__main__":
    pytest.main()
