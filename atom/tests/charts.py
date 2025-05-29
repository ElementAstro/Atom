"""
This module provides functions to generate bar, line, scatter, pie, histogram, and heatmap charts from JSON data.
Enhanced for flexibility, usability and customization.
"""

import sys
import json
import argparse
import os
import matplotlib.pyplot as plt
import numpy as np
import seaborn as sns
from matplotlib.colors import LinearSegmentedColormap
from datetime import datetime


def load_data(file_path):
    """Load JSON data from a file."""
    try:
        with open(file_path, 'r', encoding='utf-8') as f:
            return json.load(f)
    except FileNotFoundError:
        print(f"Error: File '{file_path}' not found.")
        sys.exit(1)
    except json.JSONDecodeError:
        print(f"Error: File '{file_path}' contains invalid JSON.")
        sys.exit(1)
    except Exception as e:
        print(f"Error loading JSON: {e}")
        sys.exit(1)


def validate_metric(data, metric):
    """Check if metric exists in data."""
    for suite_data in data.values():
        if suite_data and metric not in suite_data[0]:
            raise ValueError(f"Metric '{metric}' not found in data.")


def get_available_metrics(data):
    """Return a list of available metrics in the data."""
    for suite_data in data.values():
        if suite_data:
            return list(suite_data[0].keys())
    return []


def set_style(style='default', dark_mode=False):
    """Set the visual style of charts."""
    if dark_mode:
        plt.style.use('dark_background')
    elif style == 'seaborn':
        sns.set_theme()
    elif style == 'ggplot':
        plt.style.use('ggplot')
    elif style == 'minimal':
        plt.style.use('seaborn-v0_8-whitegrid')
    else:
        plt.style.use('default')


def generate_bar_chart(data, metric, output_file, show=False, style='default', dark_mode=False,
                       sort=False, horizontal=False, stacked=False, title=None):
    """Generate a bar chart for a specific metric."""
    validate_metric(data, metric)
    set_style(style, dark_mode)

    suites = list(data.keys())

    if stacked and len(next(iter(data.values()))) > 1:
        # Handle stacked bar chart
        plt.figure(figsize=(12, 8))

        # Prepare data for stacking
        num_iterations = len(next(iter(data.values())))
        bottom_values = np.zeros(len(suites))

        for i in range(num_iterations):
            iteration_values = [data[suite][i][metric] if i <
                                len(data[suite]) else 0 for suite in suites]
            plt.bar(suites, iteration_values,
                    bottom=bottom_values, label=f'Iteration {i+1}')
            bottom_values += iteration_values

        plt.legend()
    else:
        # Regular bar chart with average values
        metrics = [sum(result[metric] for result in suite_data) / len(suite_data)
                   for suite_data in data.values()]

        if sort:
            sorted_data = sorted(zip(suites, metrics), key=lambda x: x[1])
            suites = [item[0] for item in sorted_data]
            metrics = [item[1] for item in sorted_data]

        plt.figure(figsize=(12, 8))

        if horizontal:
            plt.barh(suites, metrics)
            plt.xlabel(metric)
            plt.ylabel('Suite')

            for i, v in enumerate(metrics):
                plt.text(v + max(metrics)*0.01, i, f"{v:.2f}", va='center')
        else:
            plt.bar(suites, metrics)
            plt.xlabel('Suite')
            plt.ylabel(metric)

            for i, v in enumerate(metrics):
                plt.text(i, v + max(metrics)*0.01, f"{v:.2f}", ha='center')

    chart_title = title or f'Average {metric} by Suite'
    plt.title(chart_title)
    plt.grid(axis='y', linestyle='--', alpha=0.7)
    plt.tight_layout()

    os.makedirs(os.path.dirname(os.path.abspath(output_file)), exist_ok=True)
    plt.savefig(output_file, dpi=300)
    if show:
        plt.show()
    plt.close()


def generate_line_chart(data, metric, output_file, show=False, style='default', dark_mode=False,
                        markers=True, fill=False, title=None, trend_line=False):
    """Generate a line chart for a specific metric over iterations."""
    validate_metric(data, metric)
    set_style(style, dark_mode)

    plt.figure(figsize=(12, 8))

    marker_styles = ['o', 's', 'D', '^', 'v', '<',
                     '>', 'p', '*', 'h', 'H', '+', 'x', 'd']

    for i, (suite_name, suite_data) in enumerate(data.items()):
        iterations = range(1, len(suite_data) + 1)
        metrics = [result[metric] for result in suite_data]

        marker = marker_styles[i % len(marker_styles)] if markers else None

        plt.plot(iterations, metrics, label=suite_name,
                 marker=marker, linewidth=2)

        if fill:
            plt.fill_between(iterations, 0, metrics, alpha=0.1)

        if trend_line and len(metrics) > 1:
            z = np.polyfit(iterations, metrics, 1)
            p = np.poly1d(z)
            plt.plot(iterations, p(iterations), "--", linewidth=1)

    chart_title = title or f'{metric} Over Iterations'
    plt.title(chart_title)
    plt.xlabel('Iteration')
    plt.ylabel(metric)
    plt.legend(loc='best')
    plt.grid(True, linestyle='--', alpha=0.7)
    plt.tight_layout()

    os.makedirs(os.path.dirname(os.path.abspath(output_file)), exist_ok=True)
    plt.savefig(output_file, dpi=300)
    if show:
        plt.show()
    plt.close()


def generate_scatter_chart(data, metric_x, metric_y, output_file, show=False, style='default',
                           dark_mode=False, trend_line=False, size_metric=None, title=None):
    """Generate a scatter chart for two metrics."""
    validate_metric(data, metric_x)
    validate_metric(data, metric_y)
    set_style(style, dark_mode)

    if size_metric:
        validate_metric(data, size_metric)

    plt.figure(figsize=(12, 8))

    for suite_name, suite_data in data.items():
        x = [result[metric_x] for result in suite_data]
        y = [result[metric_y] for result in suite_data]

        if size_metric:
            sizes = [result[size_metric] * 10 for result in suite_data]
            plt.scatter(x, y, s=sizes, label=suite_name, alpha=0.7)
        else:
            plt.scatter(x, y, label=suite_name)

        if trend_line and len(x) > 1:
            z = np.polyfit(x, y, 1)
            p = np.poly1d(z)
            plt.plot(sorted(x), p(sorted(x)), "--", linewidth=1)

    chart_title = title or f'{metric_y} vs {metric_x}'
    plt.title(chart_title)
    plt.xlabel(metric_x)
    plt.ylabel(metric_y)
    plt.legend()
    plt.grid(True, linestyle='--', alpha=0.7)
    plt.tight_layout()

    os.makedirs(os.path.dirname(os.path.abspath(output_file)), exist_ok=True)
    plt.savefig(output_file, dpi=300)
    if show:
        plt.show()
    plt.close()


def generate_pie_chart(data, metric, output_file, show=False, style='default', dark_mode=False,
                       explode=False, percentage=True, title=None):
    """Generate a pie chart for a specific metric."""
    validate_metric(data, metric)
    set_style(style, dark_mode)

    plt.figure(figsize=(10, 10))

    suites = list(data.keys())
    metrics = [sum(result[metric] for result in suite_data) / len(suite_data)
               for suite_data in data.values()]

    explodes = tuple(0.05 for _ in suites) if explode else None
    autopct = '%1.1f%%' if percentage else None

    plt.pie(metrics, labels=suites, autopct=autopct, explode=explodes,
            shadow=True, startangle=90)

    chart_title = title or f'Distribution of {metric} by Suite'
    plt.title(chart_title)
    plt.axis('equal')

    os.makedirs(os.path.dirname(os.path.abspath(output_file)), exist_ok=True)
    plt.savefig(output_file, dpi=300)
    if show:
        plt.show()
    plt.close()


def generate_histogram(data, metric, output_file, show=False, style='default', dark_mode=False,
                       bins=10, kde=False, title=None):
    """Generate a histogram for a specific metric."""
    validate_metric(data, metric)
    set_style(style, dark_mode)

    plt.figure(figsize=(12, 8))

    all_metrics = []
    for suite_data in data.values():
        metrics = [result[metric] for result in suite_data]
        all_metrics.extend(metrics)

    sns.histplot(all_metrics, bins=bins, kde=kde)

    chart_title = title or f'Histogram of {metric}'
    plt.title(chart_title)
    plt.xlabel(metric)
    plt.ylabel('Count')
    plt.grid(True, linestyle='--', alpha=0.7)
    plt.tight_layout()

    os.makedirs(os.path.dirname(os.path.abspath(output_file)), exist_ok=True)
    plt.savefig(output_file, dpi=300)
    if show:
        plt.show()
    plt.close()


def generate_heatmap(data, metrics, output_file, show=False, style='default', dark_mode=False, title=None):
    """Generate a heatmap for multiple metrics across suites."""
    for metric in metrics:
        validate_metric(data, metric)

    set_style(style, dark_mode)

    plt.figure(figsize=(12, 8))

    suites = list(data.keys())

    matrix = []
    for suite in suites:
        suite_data = data[suite]
        row = []
        for metric in metrics:
            avg_value = sum(result[metric]
                            for result in suite_data) / len(suite_data)
            row.append(avg_value)
        matrix.append(row)

    if dark_mode:
        cmap = LinearSegmentedColormap.from_list(
            "", ["navy", "blue", "cyan", "yellow", "red"])
    else:
        cmap = LinearSegmentedColormap.from_list(
            "", ["green", "yellow", "red"])

    sns.heatmap(matrix, annot=True, fmt=".2f", xticklabels=metrics,
                yticklabels=suites, cmap=cmap)

    chart_title = title or f'Heatmap of Metrics by Suite'
    plt.title(chart_title)
    plt.tight_layout()

    os.makedirs(os.path.dirname(os.path.abspath(output_file)), exist_ok=True)
    plt.savefig(output_file, dpi=300)
    if show:
        plt.show()
    plt.close()


def generate_all_charts(data, metrics, out_dir, show=False, style='default', dark_mode=False):
    """Generate all charts for given metrics."""
    os.makedirs(out_dir, exist_ok=True)

    for metric in metrics:
        generate_bar_chart(data, metric, os.path.join(out_dir, f'{metric}_bar.png'),
                           show=show, style=style, dark_mode=dark_mode)

        generate_line_chart(data, metric, os.path.join(out_dir, f'{metric}_line.png'),
                            show=show, style=style, dark_mode=dark_mode)

        generate_pie_chart(data, metric, os.path.join(out_dir, f'{metric}_pie.png'),
                           show=show, style=style, dark_mode=dark_mode)

        generate_histogram(data, metric, os.path.join(out_dir, f'{metric}_histogram.png'),
                           show=show, style=style, dark_mode=dark_mode)

    if len(metrics) >= 2:
        for i, metric_x in enumerate(metrics[:-1]):
            for metric_y in metrics[i+1:]:
                generate_scatter_chart(data, metric_x, metric_y,
                                       os.path.join(
                                           out_dir, f'{metric_y}_vs_{metric_x}_scatter.png'),
                                       show=show, style=style, dark_mode=dark_mode)

    generate_heatmap(data, metrics, os.path.join(out_dir, f'metrics_heatmap.png'),
                     show=show, style=style, dark_mode=dark_mode)


def generate_report(data, metrics, out_dir, style='default', dark_mode=False):
    """Generate an HTML report with all charts and statistics."""
    os.makedirs(out_dir, exist_ok=True)

    chart_dir = os.path.join(out_dir, "charts")
    generate_all_charts(data, metrics, chart_dir, False, style, dark_mode)

    stats = {}
    for metric in metrics:
        stats[metric] = {}
        for suite_name, suite_data in data.items():
            values = [result[metric] for result in suite_data]
            stats[metric][suite_name] = {
                'min': min(values),
                'max': max(values),
                'avg': sum(values) / len(values),
                'stdev': np.std(values) if len(values) > 1 else 0
            }

    now = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    html_content = f"""<!DOCTYPE html>
<html>
<head>
    <title>Performance Test Results</title>
    <style>
        body {{ font-family: Arial, sans-serif; margin: 20px; background-color: {"#222" if dark_mode else "#fff"}; color: {"#eee" if dark_mode else "#333"}; }}
        h1, h2, h3 {{ color: {"#fff" if dark_mode else "#000"}; }}
        .container {{ max-width: 1200px; margin: 0 auto; }}
        .chart {{ margin-bottom: 30px; text-align: center; }}
        .chart img {{ max-width: 100%; border: 1px solid {"#444" if dark_mode else "#ddd"}; }}
        table {{ border-collapse: collapse; width: 100%; margin-bottom: 20px; }}
        th, td {{ text-align: left; padding: 12px; border: 1px solid {"#444" if dark_mode else "#ddd"}; }}
        th {{ background-color: {"#444" if dark_mode else "#f2f2f2"}; }}
        tr:nth-child(even) {{ background-color: {"#333" if dark_mode else "#f9f9f9"}; }}
    </style>
</head>
<body>
    <div class="container">
        <h1>Performance Test Results</h1>
        <p>Generated on: {now}</p>
        
        <h2>Statistics</h2>
"""

    for metric in metrics:
        html_content += f"""
        <h3>{metric}</h3>
        <table>
            <tr>
                <th>Suite</th>
                <th>Minimum</th>
                <th>Maximum</th>
                <th>Average</th>
                <th>Standard Deviation</th>
            </tr>
"""
        for suite_name, values in stats[metric].items():
            html_content += f"""
            <tr>
                <td>{suite_name}</td>
                <td>{values['min']:.2f}</td>
                <td>{values['max']:.2f}</td>
                <td>{values['avg']:.2f}</td>
                <td>{values['stdev']:.2f}</td>
            </tr>
"""
        html_content += """
        </table>
"""

    html_content += """
        <h2>Charts</h2>
"""

    for metric in metrics:
        html_content += f"""
        <div class="chart">
            <h3>{metric} - Bar Chart</h3>
            <img src="charts/{metric}_bar.png" alt="{metric} Bar Chart">
        </div>
        
        <div class="chart">
            <h3>{metric} - Line Chart</h3>
            <img src="charts/{metric}_line.png" alt="{metric} Line Chart">
        </div>
        
        <div class="chart">
            <h3>{metric} - Pie Chart</h3>
            <img src="charts/{metric}_pie.png" alt="{metric} Pie Chart">
        </div>
        
        <div class="chart">
            <h3>{metric} - Histogram</h3>
            <img src="charts/{metric}_histogram.png" alt="{metric} Histogram">
        </div>
"""

    if len(metrics) >= 2:
        for i, metric_x in enumerate(metrics[:-1]):
            for metric_y in metrics[i+1:]:
                html_content += f"""
        <div class="chart">
            <h3>{metric_y} vs {metric_x} - Scatter Chart</h3>
            <img src="charts/{metric_y}_vs_{metric_x}_scatter.png" alt="{metric_y} vs {metric_x} Scatter Chart">
        </div>
"""

    html_content += """
        <div class="chart">
            <h3>Metrics Heatmap</h3>
            <img src="charts/metrics_heatmap.png" alt="Metrics Heatmap">
        </div>
    </div>
</body>
</html>
"""

    report_path = os.path.join(out_dir, "report.html")
    with open(report_path, 'w', encoding='utf-8') as f:
        f.write(html_content)

    return report_path


def main():
    """Main function for command-line interface."""
    parser = argparse.ArgumentParser(
        description="Generate charts from JSON data.")
    parser.add_argument("json_file", help="Path to JSON file")
    parser.add_argument("--metrics", nargs="+",
                        help="Metrics to plot (default: auto-detect from data)")
    parser.add_argument("--out-dir", default="charts",
                        help="Output directory for charts")
    parser.add_argument("--show", action="store_true",
                        help="Show charts interactively")
    parser.add_argument("--chart-type", choices=["bar", "line", "scatter", "pie", "histogram", "heatmap", "all"],
                        default="all", help="Type of chart to generate")
    parser.add_argument("--scatter-metrics", nargs=2, metavar=('X', 'Y'),
                        help="Generate scatter chart for two specific metrics")
    parser.add_argument("--style", choices=["default", "seaborn", "ggplot", "minimal"],
                        default="default", help="Chart style")
    parser.add_argument("--dark-mode", action="store_true",
                        help="Use dark mode for charts")
    parser.add_argument("--trend-line", action="store_true",
                        help="Add trend lines to line and scatter charts")
    parser.add_argument("--report", action="store_true",
                        help="Generate HTML report with all charts and statistics")
    parser.add_argument("--list-metrics", action="store_true",
                        help="List all available metrics in the data")

    args = parser.parse_args()

    data = load_data(args.json_file)

    if args.metrics is None:
        available_metrics = get_available_metrics(data)
        if args.list_metrics:
            print("Available metrics:")
            for metric in available_metrics:
                print(f"  - {metric}")
            return
        args.metrics = available_metrics[:3] if available_metrics else [
            "averageDuration", "throughput", "peakMemoryUsage"]

    if args.report:
        report_path = generate_report(
            data, args.metrics, args.out_dir, args.style, args.dark_mode)
        print(f"Report generated: {report_path}")
        return

    os.makedirs(args.out_dir, exist_ok=True)

    if args.chart_type == "bar" or args.chart_type == "all":
        for metric in args.metrics:
            output_file = os.path.join(args.out_dir, f'{metric}_bar.png')
            generate_bar_chart(data, metric, output_file,
                               args.show, args.style, args.dark_mode)
            print(f"Generated: {output_file}")

    if args.chart_type == "line" or args.chart_type == "all":
        for metric in args.metrics:
            output_file = os.path.join(args.out_dir, f'{metric}_line.png')
            generate_line_chart(data, metric, output_file, args.show,
                                args.style, args.dark_mode, trend_line=args.trend_line)
            print(f"Generated: {output_file}")

    if args.chart_type == "scatter" or (args.chart_type == "all" and len(args.metrics) >= 2):
        if args.scatter_metrics:
            output_file = os.path.join(
                args.out_dir, f'{args.scatter_metrics[1]}_vs_{args.scatter_metrics[0]}_scatter.png')
            generate_scatter_chart(data, args.scatter_metrics[0], args.scatter_metrics[1],
                                   output_file, args.show, args.style, args.dark_mode, trend_line=args.trend_line)
            print(f"Generated: {output_file}")
        else:
            for i, metric_x in enumerate(args.metrics[:-1]):
                for metric_y in args.metrics[i+1:]:
                    output_file = os.path.join(
                        args.out_dir, f'{metric_y}_vs_{metric_x}_scatter.png')
                    generate_scatter_chart(data, metric_x, metric_y, output_file,
                                           args.show, args.style, args.dark_mode, trend_line=args.trend_line)
                    print(f"Generated: {output_file}")

    if args.chart_type == "pie" or args.chart_type == "all":
        for metric in args.metrics:
            output_file = os.path.join(args.out_dir, f'{metric}_pie.png')
            generate_pie_chart(data, metric, output_file,
                               args.show, args.style, args.dark_mode)
            print(f"Generated: {output_file}")

    if args.chart_type == "histogram" or args.chart_type == "all":
        for metric in args.metrics:
            output_file = os.path.join(args.out_dir, f'{metric}_histogram.png')
            generate_histogram(data, metric, output_file,
                               args.show, args.style, args.dark_mode)
            print(f"Generated: {output_file}")

    if args.chart_type == "heatmap" or args.chart_type == "all":
        output_file = os.path.join(args.out_dir, f'metrics_heatmap.png')
        generate_heatmap(data, args.metrics, output_file,
                         args.show, args.style, args.dark_mode)
        print(f"Generated: {output_file}")


class ChartGenerator:
    """Python API for direct use."""

    def __init__(self, data=None, json_file=None, style='default', dark_mode=False):
        """Initialize ChartGenerator with data or JSON file."""
        if data is not None:
            self.data = data
        elif json_file is not None:
            self.data = load_data(json_file)
        else:
            raise ValueError("Either data or json_file must be provided")

        self.style = style
        self.dark_mode = dark_mode
        self.metrics = get_available_metrics(self.data)

    def bar_chart(self, metric, output_file=None, show=False, **kwargs):
        """Generate a bar chart."""
        output_file = output_file or f'{metric}_bar.png'
        generate_bar_chart(self.data, metric, output_file,
                           show, self.style, self.dark_mode, **kwargs)
        return output_file

    def line_chart(self, metric, output_file=None, show=False, **kwargs):
        """Generate a line chart."""
        output_file = output_file or f'{metric}_line.png'
        generate_line_chart(self.data, metric, output_file,
                            show, self.style, self.dark_mode, **kwargs)
        return output_file

    def scatter_chart(self, metric_x, metric_y, output_file=None, show=False, **kwargs):
        """Generate a scatter chart."""
        output_file = output_file or f'{metric_y}_vs_{metric_x}_scatter.png'
        generate_scatter_chart(self.data, metric_x, metric_y,
                               output_file, show, self.style, self.dark_mode, **kwargs)
        return output_file

    def pie_chart(self, metric, output_file=None, show=False, **kwargs):
        """Generate a pie chart."""
        output_file = output_file or f'{metric}_pie.png'
        generate_pie_chart(self.data, metric, output_file,
                           show, self.style, self.dark_mode, **kwargs)
        return output_file

    def histogram(self, metric, output_file=None, show=False, **kwargs):
        """Generate a histogram."""
        output_file = output_file or f'{metric}_histogram.png'
        generate_histogram(self.data, metric, output_file,
                           show, self.style, self.dark_mode, **kwargs)
        return output_file

    def heatmap(self, metrics=None, output_file=None, show=False, **kwargs):
        """Generate a heatmap."""
        metrics = metrics or self.metrics
        output_file = output_file or 'metrics_heatmap.png'
        generate_heatmap(self.data, metrics, output_file, show,
                         self.style, self.dark_mode, **kwargs)
        return output_file

    def all_charts(self, metrics=None, out_dir="charts", show=False):
        """Generate all charts."""
        metrics = metrics or self.metrics
        generate_all_charts(self.data, metrics, out_dir,
                            show, self.style, self.dark_mode)
        return out_dir

    def generate_report(self, metrics=None, out_dir="report"):
        """Generate an HTML report."""
        metrics = metrics or self.metrics
        return generate_report(self.data, metrics, out_dir, self.style, self.dark_mode)


def plot_from_json(json_file, metrics=None, out_dir="charts", show=False, style='default', dark_mode=False):
    """Generate all charts from JSON file."""
    data = load_data(json_file)
    if metrics is None:
        metrics = get_available_metrics(data)
    generate_all_charts(data, metrics, out_dir, show, style, dark_mode)


if __name__ == "__main__":
    main()
