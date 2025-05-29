import os
import json
import tempfile
import pytest
from unittest.mock import patch, MagicMock, mock_open
from io import StringIO
import sys

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
    """Create sample data for testing chart generation functions."""
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
    """Create a temporary JSON file with sample data for testing file operations."""
    with tempfile.NamedTemporaryFile(suffix='.json', delete=False, mode='w') as f:
        json.dump(sample_data, f)
        filename = f.name
    yield filename
    os.remove(filename)


@pytest.fixture
def output_dir():
    """Create a temporary directory for testing output file generation."""
    with tempfile.TemporaryDirectory() as tmpdirname:
        yield tmpdirname


class TestDataLoading:
    """Test suite for data loading functionality."""

    def test_load_data_success(self, json_file):
        """Test successful loading of JSON data."""
        data = load_data(json_file)
        assert "suite1" in data
        assert "suite2" in data
        assert len(data["suite1"]) == 3
        assert data["suite1"][0]["metric1"] == 10

    def test_load_data_file_not_found(self):
        """Test handling of non-existent file."""
        with pytest.raises(SystemExit) as exc_info:
            load_data("nonexistent_file.json")
        assert exc_info.value.code == 1

    def test_load_data_invalid_json(self):
        """Test handling of invalid JSON content."""
        with patch("builtins.open", mock_open(read_data="{invalid json")):
            with pytest.raises(SystemExit) as exc_info:
                load_data("invalid.json")
            assert exc_info.value.code == 1


class TestMetricValidation:
    """Test suite for metric validation functionality."""

    def test_validate_metric_valid(self, sample_data):
        """Test validation of existing metrics."""
        for metric in ["metric1", "metric2", "metric3"]:
            validate_metric(sample_data, metric)

    def test_validate_metric_invalid(self, sample_data):
        """Test validation of non-existent metric."""
        with pytest.raises(ValueError, match="not found in data"):
            validate_metric(sample_data, "nonexistent_metric")

    def test_get_available_metrics(self, sample_data):
        """Test retrieval of available metrics."""
        metrics = get_available_metrics(sample_data)
        expected_metrics = {"metric1", "metric2", "metric3"}
        assert set(metrics) == expected_metrics
        assert len(metrics) == 3

    def test_get_available_metrics_empty_data(self):
        """Test metrics retrieval with empty data."""
        metrics = get_available_metrics({})
        assert metrics == []


class TestStyleConfiguration:
    """Test suite for chart styling functionality."""

    @patch("matplotlib.pyplot.style.use")
    def test_set_style_default(self, mock_style_use):
        """Test default style configuration."""
        set_style()
        mock_style_use.assert_called_once_with('default')

    @patch("matplotlib.pyplot.style.use")
    def test_set_style_dark_mode(self, mock_style_use):
        """Test dark mode style configuration."""
        set_style(dark_mode=True)
        mock_style_use.assert_called_once_with('dark_background')

    @patch("matplotlib.pyplot.style.use")
    def test_set_style_seaborn(self, mock_style_use):
        """Test seaborn style configuration."""
        set_style(style="seaborn")
        mock_style_use.assert_not_called()

    @patch("seaborn.set_theme")
    def test_set_style_seaborn_theme(self, mock_set_theme):
        """Test seaborn theme application."""
        set_style(style="seaborn")
        mock_set_theme.assert_called_once()


class TestChartGeneration:
    """Test suite for individual chart generation functions."""

    @pytest.fixture(autouse=True)
    def setup_mocks(self):
        """Set up common mocks for chart generation tests."""
        with patch("matplotlib.pyplot.figure", return_value=MagicMock()) as mock_figure, \
                patch("matplotlib.pyplot.savefig") as mock_savefig, \
                patch("matplotlib.pyplot.close") as mock_close, \
                patch("os.makedirs") as mock_makedirs:
            self.mock_figure = mock_figure
            self.mock_savefig = mock_savefig
            self.mock_close = mock_close
            self.mock_makedirs = mock_makedirs
            yield

    def test_generate_bar_chart(self, sample_data, output_dir):
        """Test bar chart generation."""
        output_file = os.path.join(output_dir, "test_bar.png")
        generate_bar_chart(sample_data, "metric1", output_file)

        self.mock_makedirs.assert_called_once()
        self.mock_savefig.assert_called_once_with(output_file, dpi=300)
        self.mock_close.assert_called_once()

    def test_generate_line_chart(self, sample_data, output_dir):
        """Test line chart generation."""
        output_file = os.path.join(output_dir, "test_line.png")
        generate_line_chart(sample_data, "metric1", output_file)

        self.mock_makedirs.assert_called_once()
        self.mock_savefig.assert_called_once_with(output_file, dpi=300)
        self.mock_close.assert_called_once()

    def test_generate_scatter_chart(self, sample_data, output_dir):
        """Test scatter chart generation."""
        output_file = os.path.join(output_dir, "test_scatter.png")
        generate_scatter_chart(sample_data, "metric1", "metric2", output_file)

        self.mock_makedirs.assert_called_once()
        self.mock_savefig.assert_called_once_with(output_file, dpi=300)
        self.mock_close.assert_called_once()

    def test_generate_pie_chart(self, sample_data, output_dir):
        """Test pie chart generation."""
        output_file = os.path.join(output_dir, "test_pie.png")
        generate_pie_chart(sample_data, "metric1", output_file)

        self.mock_makedirs.assert_called_once()
        self.mock_savefig.assert_called_once_with(output_file, dpi=300)
        self.mock_close.assert_called_once()

    @patch("seaborn.histplot")
    def test_generate_histogram(self, mock_histplot, sample_data, output_dir):
        """Test histogram generation."""
        output_file = os.path.join(output_dir, "test_histogram.png")
        generate_histogram(sample_data, "metric1", output_file)

        self.mock_makedirs.assert_called_once()
        self.mock_savefig.assert_called_once_with(output_file, dpi=300)
        self.mock_close.assert_called_once()
        mock_histplot.assert_called_once()

    @patch("seaborn.heatmap")
    def test_generate_heatmap(self, mock_heatmap, sample_data, output_dir):
        """Test heatmap generation."""
        output_file = os.path.join(output_dir, "test_heatmap.png")
        generate_heatmap(sample_data, ["metric1", "metric2"], output_file)

        self.mock_makedirs.assert_called_once()
        self.mock_savefig.assert_called_once_with(output_file, dpi=300)
        self.mock_close.assert_called_once()
        mock_heatmap.assert_called_once()


class TestBulkOperations:
    """Test suite for bulk chart generation operations."""

    @patch("atom.tests.charts.generate_bar_chart")
    @patch("atom.tests.charts.generate_line_chart")
    @patch("atom.tests.charts.generate_pie_chart")
    @patch("atom.tests.charts.generate_histogram")
    @patch("atom.tests.charts.generate_scatter_chart")
    @patch("atom.tests.charts.generate_heatmap")
    @patch("os.makedirs")
    def test_generate_all_charts(self, mock_makedirs, mock_heatmap, mock_scatter,
                                 mock_histogram, mock_pie, mock_line, mock_bar,
                                 sample_data, output_dir):
        """Test generation of all chart types."""
        metrics = ["metric1", "metric2", "metric3"]
        generate_all_charts(sample_data, metrics, output_dir)

        assert mock_bar.call_count == 3
        assert mock_line.call_count == 3
        assert mock_pie.call_count == 3
        assert mock_histogram.call_count == 3
        assert mock_scatter.call_count == 3
        assert mock_heatmap.call_count == 1
        mock_makedirs.assert_called_once_with(output_dir, exist_ok=True)

    @patch("atom.tests.charts.generate_all_charts")
    @patch("builtins.open", new_callable=mock_open)
    @patch("os.makedirs")
    def test_generate_report(self, mock_makedirs, mock_file_open,
                             mock_gen_all_charts, sample_data, output_dir):
        """Test HTML report generation."""
        metrics = ["metric1", "metric2"]
        report_path = generate_report(sample_data, metrics, output_dir)

        expected_path = os.path.join(output_dir, "report.html")
        assert report_path == expected_path
        mock_makedirs.assert_called()
        mock_gen_all_charts.assert_called_once()
        mock_file_open.assert_called()


class TestChartGeneratorClass:
    """Test suite for ChartGenerator class functionality."""

    @patch("atom.tests.charts.load_data")
    def test_init_with_json_file(self, mock_load_data, sample_data):
        """Test ChartGenerator initialization with JSON file."""
        mock_load_data.return_value = sample_data
        generator = ChartGenerator(json_file="test.json")

        mock_load_data.assert_called_once_with("test.json")
        assert generator.metrics == list(sample_data["suite1"][0].keys())

    def test_init_with_data(self, sample_data):
        """Test ChartGenerator initialization with direct data."""
        generator = ChartGenerator(data=sample_data)

        assert generator.data == sample_data
        assert generator.style == "default"
        assert not generator.dark_mode

    def test_init_without_data_or_file(self):
        """Test ChartGenerator initialization error handling."""
        with pytest.raises(ValueError, match="Either data or json_file must be provided"):
            ChartGenerator()

    @pytest.mark.parametrize("method_name,chart_type,expected_suffix", [
        ("bar_chart", "bar", "_bar.png"),
        ("line_chart", "line", "_line.png"),
        ("pie_chart", "pie", "_pie.png"),
        ("histogram", "histogram", "_histogram.png"),
    ])
    @patch("atom.tests.charts.generate_bar_chart")
    @patch("atom.tests.charts.generate_line_chart")
    @patch("atom.tests.charts.generate_pie_chart")
    @patch("atom.tests.charts.generate_histogram")
    def test_single_metric_chart_methods(self, mock_histogram, mock_pie, mock_line,
                                         mock_bar, method_name, chart_type,
                                         expected_suffix, sample_data):
        """Test individual chart generation methods."""
        generator = ChartGenerator(data=sample_data)
        method = getattr(generator, method_name)
        output_file = method("metric1")

        assert output_file == f"metric1{expected_suffix}"

        mock_map = {
            "bar_chart": mock_bar,
            "line_chart": mock_line,
            "pie_chart": mock_pie,
            "histogram": mock_histogram
        }
        mock_map[method_name].assert_called_once()

    @patch("atom.tests.charts.generate_scatter_chart")
    def test_scatter_chart_method(self, mock_scatter_chart, sample_data):
        """Test scatter chart generation method."""
        generator = ChartGenerator(data=sample_data)
        output_file = generator.scatter_chart("metric1", "metric2")

        mock_scatter_chart.assert_called_once()
        assert output_file == "metric2_vs_metric1_scatter.png"

    @patch("atom.tests.charts.generate_heatmap")
    def test_heatmap_method(self, mock_heatmap, sample_data):
        """Test heatmap generation method."""
        generator = ChartGenerator(data=sample_data)
        output_file = generator.heatmap()

        mock_heatmap.assert_called_once()
        assert output_file == "metrics_heatmap.png"

    @patch("atom.tests.charts.generate_all_charts")
    def test_all_charts_method(self, mock_all_charts, sample_data):
        """Test all charts generation method."""
        generator = ChartGenerator(data=sample_data)
        output_dir = generator.all_charts()

        mock_all_charts.assert_called_once()
        assert output_dir == "charts"

    @patch("atom.tests.charts.generate_report")
    def test_generate_report_method(self, mock_gen_report, sample_data):
        """Test report generation method."""
        generator = ChartGenerator(data=sample_data)
        generator.generate_report()

        mock_gen_report.assert_called_once()


class TestUtilityFunctions:
    """Test suite for utility and helper functions."""

    @patch("atom.tests.charts.load_data")
    @patch("atom.tests.charts.get_available_metrics")
    @patch("atom.tests.charts.generate_all_charts")
    def test_plot_from_json(self, mock_gen_all_charts, mock_get_metrics,
                            mock_load_data, sample_data):
        """Test JSON-based plotting function."""
        mock_load_data.return_value = sample_data
        mock_get_metrics.return_value = ["metric1", "metric2", "metric3"]

        plot_from_json("test.json")

        mock_load_data.assert_called_once_with("test.json")
        mock_get_metrics.assert_called_once()
        mock_gen_all_charts.assert_called_once()


class TestMainFunction:
    """Test suite for command-line interface functionality."""

    @patch("atom.tests.charts.load_data")
    @patch("atom.tests.charts.get_available_metrics")
    @patch("atom.tests.charts.generate_bar_chart")
    @patch("atom.tests.charts.generate_line_chart")
    @patch("atom.tests.charts.generate_pie_chart")
    @patch("atom.tests.charts.generate_histogram")
    @patch("atom.tests.charts.generate_heatmap")
    @patch("os.makedirs")
    def test_main_default_arguments(self, mock_makedirs, mock_heatmap, mock_histogram,
                                    mock_pie, mock_line, mock_bar, mock_get_metrics,
                                    mock_load_data, sample_data):
        """Test main function with default arguments."""
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

    @patch("atom.tests.charts.load_data")
    @patch("atom.tests.charts.get_available_metrics")
    @patch("atom.tests.charts.generate_bar_chart")
    @patch("os.makedirs")
    def test_main_specific_chart_type(self, mock_makedirs, mock_bar,
                                      mock_get_metrics, mock_load_data, sample_data):
        """Test main function with specific chart type."""
        mock_load_data.return_value = sample_data
        mock_get_metrics.return_value = ["metric1", "metric2", "metric3"]

        with patch.object(sys, 'argv', ["charts.py", "test.json", "--chart-type", "bar"]):
            main()

        assert mock_bar.call_count == 3  # One for each metric

    @patch("atom.tests.charts.load_data")
    @patch("atom.tests.charts.generate_bar_chart")
    @patch("atom.tests.charts.generate_line_chart")
    @patch("atom.tests.charts.generate_scatter_chart")
    @patch("atom.tests.charts.generate_pie_chart")
    @patch("atom.tests.charts.generate_histogram")
    @patch("atom.tests.charts.generate_heatmap")
    @patch("os.makedirs")
    def test_main_specific_metrics(self, mock_makedirs, mock_heatmap, mock_histogram,
                                   mock_pie, mock_scatter, mock_line, mock_bar,
                                   mock_load_data, sample_data):
        """Test main function with specific metrics."""
        mock_load_data.return_value = sample_data

        with patch.object(sys, 'argv', ["charts.py", "test.json", "--metrics", "metric1", "metric2"]):
            main()

        assert mock_bar.call_count == 2
        assert mock_line.call_count == 2
        assert mock_pie.call_count == 2
        assert mock_histogram.call_count == 2
        mock_heatmap.assert_called_once()

    @patch("atom.tests.charts.load_data")
    @patch("atom.tests.charts.get_available_metrics")
    @patch("atom.tests.charts.generate_report")
    def test_main_generate_report_option(self, mock_gen_report, mock_get_metrics,
                                         mock_load_data, sample_data):
        """Test main function with report generation option."""
        mock_load_data.return_value = sample_data
        mock_get_metrics.return_value = ["metric1", "metric2", "metric3"]
        mock_gen_report.return_value = "/path/to/report.html"

        with patch.object(sys, 'argv', ["charts.py", "test.json", "--report"]):
            with patch('sys.stdout', new=StringIO()) as fake_output:
                main()
                assert "Report generated" in fake_output.getvalue()

        mock_gen_report.assert_called_once()

    @patch("atom.tests.charts.load_data")
    @patch("atom.tests.charts.get_available_metrics")
    def test_main_list_metrics_option(self, mock_get_metrics, mock_load_data, sample_data):
        """Test main function with list metrics option."""
        mock_load_data.return_value = sample_data
        mock_get_metrics.return_value = ["metric1", "metric2", "metric3"]

        with patch.object(sys, 'argv', ["charts.py", "test.json", "--list-metrics"]):
            with patch('sys.stdout', new=StringIO()) as fake_output:
                main()
                output = fake_output.getvalue()
                assert "Available metrics:" in output
                for metric in ["metric1", "metric2", "metric3"]:
                    assert metric in output

        mock_get_metrics.assert_called_once()

    @patch("atom.tests.charts.load_data")
    @patch("atom.tests.charts.generate_scatter_chart")
    @patch("os.makedirs")
    def test_main_scatter_metrics_option(self, mock_makedirs, mock_scatter,
                                         mock_load_data, sample_data):
        """Test main function with specific scatter metrics."""
        mock_load_data.return_value = sample_data

        with patch.object(sys, 'argv', ["charts.py", "test.json", "--scatter-metrics", "metric1", "metric2"]):
            main()

        mock_scatter.assert_called_once()
        args = mock_scatter.call_args[0]
        assert args[1] == "metric1"
        assert args[2] == "metric2"


if __name__ == "__main__":
    pytest.main([__file__])
