"""
High-performance chart generation module for JSON performance data.
Optimized for speed, memory efficiency, and modern Python practices.

Features:
- Concurrent chart generation
- Memory-efficient data processing
- Comprehensive error handling and validation
- Progress tracking for long operations
- Caching for improved performance
- Type safety with comprehensive type hints
"""

from __future__ import annotations

import argparse
import asyncio
import json
import logging
import os
import sys
import warnings
from argparse import ArgumentParser, Namespace
from concurrent.futures import ProcessPoolExecutor, ThreadPoolExecutor, as_completed
from contextlib import contextmanager
from dataclasses import dataclass, field
from datetime import datetime
from functools import lru_cache, wraps
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple, Union, Iterator
from collections import defaultdict

import matplotlib
import matplotlib.pyplot as plt
import matplotlib.cm as cm
import numpy as np
import seaborn as sns
from matplotlib.colors import LinearSegmentedColormap
from matplotlib.figure import Figure
try:
    from tqdm import tqdm
except ImportError:
    # Fallback if tqdm is not available
    def tqdm(iterable, *args, **kwargs):
        return iterable

# Configure matplotlib for better performance
matplotlib.use('Agg')  # Non-interactive backend for better performance
plt.ioff()  # Turn off interactive mode

# Suppress warnings for cleaner output
warnings.filterwarnings('ignore', category=UserWarning)
warnings.filterwarnings('ignore', category=FutureWarning)

# Configure logging
logging.basicConfig(level=logging.INFO,
                    format='%(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)


# Type definitions
DataPoint = Dict[str, Union[int, float]]
SuiteData = List[DataPoint]
PerformanceData = Dict[str, SuiteData]
ChartConfig = Dict[str, Any]


@dataclass(frozen=True)
class ChartStyle:
    """Immutable chart style configuration."""
    name: str = 'default'
    dark_mode: bool = False
    dpi: int = 300
    figure_size: Tuple[int, int] = (12, 8)

    def __post_init__(self):
        """Validate configuration after initialization."""
        if self.dpi < 72 or self.dpi > 600:
            raise ValueError(f"DPI must be between 72 and 600, got {self.dpi}")
        if any(s <= 0 for s in self.figure_size):
            raise ValueError(
                f"Figure size must be positive, got {self.figure_size}")


@dataclass
class PerformanceStats:
    """Statistics for a specific metric and suite."""
    min_val: float
    max_val: float
    avg_val: float
    std_dev: float
    count: int

    @classmethod
    def from_values(cls, values: List[float]) -> PerformanceStats:
        """Create statistics from a list of values."""
        if not values:
            raise ValueError(
                "Cannot compute statistics from empty values list")

        values_array = np.array(values)
        return cls(
            min_val=float(np.min(values_array)),
            max_val=float(np.max(values_array)),
            avg_val=float(np.mean(values_array)),
            std_dev=float(np.std(values_array)) if len(values) > 1 else 0.0,
            count=len(values)
        )


class PerformanceDataError(Exception):
    """Custom exception for performance data related errors."""
    pass


class ChartGenerationError(Exception):
    """Custom exception for chart generation errors."""
    pass


@contextmanager
def performance_timer(operation_name: str):
    """Context manager for timing operations."""
    start_time = datetime.now()
    logger.info(f"Starting {operation_name}")
    try:
        yield
    finally:
        duration = (datetime.now() - start_time).total_seconds()
        logger.info(f"Completed {operation_name} in {duration:.2f}s")


def validate_data_integrity(func):
    """Decorator to validate data integrity before chart generation."""
    @wraps(func)
    def wrapper(data: PerformanceData, *args, **kwargs):
        if not data:
            raise PerformanceDataError("Data cannot be empty")

        for suite_name, suite_data in data.items():
            if not suite_data:
                raise PerformanceDataError(
                    f"Suite '{suite_name}' has no data points")

            # Validate data structure consistency
            first_keys = set(suite_data[0].keys())
            for i, point in enumerate(suite_data[1:], 1):
                if set(point.keys()) != first_keys:
                    raise PerformanceDataError(
                        f"Suite '{suite_name}' has inconsistent data structure at index {i}"
                    )

        return func(data, *args, **kwargs)

    return wrapper


@lru_cache(maxsize=128)
def load_data(file_path: str) -> PerformanceData:
    """Load and cache JSON data from a file with comprehensive error handling."""
    file_path_obj = Path(file_path)

    if not file_path_obj.exists():
        raise FileNotFoundError(f"File '{file_path}' not found")

    if not file_path_obj.is_file():
        raise ValueError(f"Path '{file_path}' is not a file")

    try:
        with performance_timer(f"loading data from {file_path}"):
            with file_path_obj.open('r', encoding='utf-8') as f:
                data = json.load(f)

            # Validate loaded data structure
            if not isinstance(data, dict):
                raise PerformanceDataError("Root data must be a dictionary")

            validated_data: PerformanceData = {}
            for suite_name, suite_data in data.items():
                if not isinstance(suite_data, list):
                    raise PerformanceDataError(
                        f"Suite '{suite_name}' must contain a list of data points")

                validated_suite_data: SuiteData = []
                for point in suite_data:
                    if not isinstance(point, dict):
                        raise PerformanceDataError(
                            f"Data points in suite '{suite_name}' must be dictionaries")
                    validated_suite_data.append(point)

                validated_data[suite_name] = validated_suite_data

            logger.info(f"Successfully loaded {len(validated_data)} suites with "
                        f"{sum(len(suite) for suite in validated_data.values())} total data points")

            return validated_data

    except json.JSONDecodeError as e:
        raise PerformanceDataError(f"Invalid JSON in file '{file_path}': {e}")
    except Exception as e:
        raise PerformanceDataError(
            f"Error loading data from '{file_path}': {e}")


@lru_cache(maxsize=32)
def get_available_metrics(data_hash: int, data_keys: Tuple[str, ...]) -> List[str]:
    """Get available metrics from data with caching."""
    # This is a workaround since we can't hash the data dict directly
    # In practice, we'll need to reconstruct or pass the data differently
    return []  # Placeholder - will be overridden by non-cached version


def get_available_metrics_uncached(data: PerformanceData) -> List[str]:
    """Get available metrics from the performance data."""
    if not data:
        return []

    for suite_data in data.values():
        if suite_data:
            return sorted(suite_data[0].keys())

    return []


def validate_metric(data: PerformanceData, metric: str) -> None:
    """Validate that a metric exists in the data."""
    available_metrics = get_available_metrics_uncached(data)
    if metric not in available_metrics:
        raise PerformanceDataError(
            f"Metric '{metric}' not found. Available metrics: {', '.join(available_metrics)}"
        )


@lru_cache(maxsize=64)
def compute_statistics(data_tuple: Tuple[Tuple[str, Tuple[float, ...]], ...],
                       metric: str) -> Dict[str, PerformanceStats]:
    """Compute cached statistics for a metric across all suites."""
    stats = {}
    for suite_name, values in data_tuple:
        stats[suite_name] = PerformanceStats.from_values(list(values))
    return stats


def get_metric_values(data: PerformanceData, metric: str) -> Dict[str, List[float]]:
    """Extract metric values from data with efficient numpy operations."""
    validate_metric(data, metric)

    result = {}
    for suite_name, suite_data in data.items():
        values = [float(point[metric]) for point in suite_data]
        result[suite_name] = values

    return result


@lru_cache(maxsize=16)
def create_style_config(style_name: str, dark_mode: bool) -> None:
    """Apply and cache matplotlib style configuration."""
    if dark_mode:
        plt.style.use('dark_background')
        sns.set_palette("husl")
    elif style_name == 'seaborn':
        sns.set_theme()
    elif style_name == 'ggplot':
        plt.style.use('ggplot')
    elif style_name == 'minimal':
        try:
            plt.style.use('seaborn-v0_8-whitegrid')
        except OSError:
            # Fallback for older matplotlib versions
            plt.style.use('seaborn-whitegrid')
    else:
        plt.style.use('default')


def ensure_output_directory(file_path: Union[str, Path]) -> Path:
    """Ensure output directory exists and return Path object."""
    path_obj = Path(file_path)
    path_obj.parent.mkdir(parents=True, exist_ok=True)
    return path_obj


@validate_data_integrity
def generate_bar_chart(data: PerformanceData, metric: str, output_file: Union[str, Path],
                       show: bool = False, style: Optional[ChartStyle] = None,
                       sort_values: bool = False, horizontal: bool = False,
                       stacked: bool = False, title: Optional[str] = None) -> Path:
    """Generate an optimized bar chart for a specific metric."""
    if style is None:
        style = ChartStyle()

    validate_metric(data, metric)
    create_style_config(style.name, style.dark_mode)

    output_path = ensure_output_directory(output_file)

    with performance_timer(f"generating bar chart for {metric}"):
        metric_values = get_metric_values(data, metric)
        suites = list(metric_values.keys())

        if stacked and any(len(values) > 1 for values in metric_values.values()):
            # Optimized stacked bar chart
            fig, ax = plt.subplots(figsize=style.figure_size)

            max_iterations = max(len(values)
                                 for values in metric_values.values())
            bottom_values = np.zeros(len(suites))

            for i in range(max_iterations):
                iteration_values = np.array([
                    values[i] if i < len(values) else 0
                    for values in metric_values.values()
                ])
                ax.bar(suites, iteration_values, bottom=bottom_values,
                       label=f'Iteration {i+1}', alpha=0.8)
                bottom_values += iteration_values

            ax.legend(bbox_to_anchor=(1.05, 1), loc='upper left')

        else:
            # Optimized regular bar chart with vectorized operations
            avg_values = np.array([np.mean(values)
                                  for values in metric_values.values()])

            if sort_values:
                sort_indices = np.argsort(avg_values)
                suites = [suites[i] for i in sort_indices]
                avg_values = avg_values[sort_indices]

            fig, ax = plt.subplots(figsize=style.figure_size)

            if horizontal:
                bars = ax.barh(suites, avg_values, alpha=0.8)
                ax.set_xlabel(metric)
                ax.set_ylabel('Suite')

                # Add value labels
                for i, (bar, value) in enumerate(zip(bars, avg_values)):
                    ax.text(value + np.max(avg_values) * 0.01, i,
                            f"{value:.2f}", va='center', fontsize=9)
            else:
                bars = ax.bar(suites, avg_values, alpha=0.8)
                ax.set_xlabel('Suite')
                ax.set_ylabel(metric)

                # Add value labels
                for bar, value in zip(bars, avg_values):
                    ax.text(bar.get_x() + bar.get_width()/2,
                            value + np.max(avg_values) * 0.01,
                            f"{value:.2f}", ha='center', fontsize=9)

        # Styling
        chart_title = title or f'Average {metric} by Suite'
        ax.set_title(chart_title, fontsize=14, fontweight='bold')
        ax.grid(axis='y', linestyle='--', alpha=0.3)

        # Rotate labels if they're too long
        if not horizontal:
            ax.tick_params(axis='x', rotation=45)

        plt.tight_layout()
        plt.savefig(output_path, dpi=style.dpi, bbox_inches='tight')

        if show:
            plt.show()
        else:
            plt.close(fig)

    logger.info(f"Generated bar chart: {output_path}")
    return output_path


@validate_data_integrity
def generate_line_chart(data: PerformanceData, metric: str, output_file: Union[str, Path],
                        show: bool = False, style: Optional[ChartStyle] = None,
                        markers: bool = True, fill: bool = False,
                        title: Optional[str] = None, trend_line: bool = False) -> Path:
    """Generate an optimized line chart for a specific metric over iterations."""
    if style is None:
        style = ChartStyle()

    validate_metric(data, metric)
    create_style_config(style.name, style.dark_mode)

    output_path = ensure_output_directory(output_file)

    with performance_timer(f"generating line chart for {metric}"):
        metric_values = get_metric_values(data, metric)

        fig, ax = plt.subplots(figsize=style.figure_size)

        # Optimized marker styles
        marker_styles = ['o', 's', 'D', '^', 'v', '<',
                         '>', 'p', '*', 'h', 'H', '+', 'x', 'd']
        colors = cm.get_cmap('tab10')(np.linspace(0, 1, len(metric_values)))

        for i, (suite_name, values) in enumerate(metric_values.items()):
            iterations = np.arange(1, len(values) + 1)
            values_array = np.array(values)

            marker = marker_styles[i % len(marker_styles)] if markers else None
            color = colors[i % len(colors)]

            ax.plot(iterations, values_array, label=suite_name, marker=marker,
                    linewidth=2, color=color, alpha=0.8)

            if fill:
                ax.fill_between(iterations, 0, values_array,
                                alpha=0.1, color=color)

            if trend_line and len(values) > 1:
                # Vectorized trend line calculation
                z = np.polyfit(iterations, values_array, 1)
                trend_values = np.polyval(z, iterations)
                ax.plot(iterations, trend_values, "--", linewidth=1,
                        color=color, alpha=0.6)

        # Styling
        chart_title = title or f'{metric} Over Iterations'
        ax.set_title(chart_title, fontsize=14, fontweight='bold')
        ax.set_xlabel('Iteration')
        ax.set_ylabel(metric)
        ax.legend(bbox_to_anchor=(1.05, 1), loc='upper left')
        ax.grid(True, linestyle='--', alpha=0.3)

        plt.tight_layout()
        plt.savefig(output_path, dpi=style.dpi, bbox_inches='tight')

        if show:
            plt.show()
        else:
            plt.close(fig)

    logger.info(f"Generated line chart: {output_path}")
    return output_path


@validate_data_integrity
def generate_scatter_chart(data: PerformanceData, metric_x: str, metric_y: str,
                           output_file: Union[str, Path], show: bool = False,
                           style: Optional[ChartStyle] = None, trend_line: bool = False,
                           size_metric: Optional[str] = None, title: Optional[str] = None) -> Path:
    """Generate an optimized scatter chart for two metrics."""
    if style is None:
        style = ChartStyle()

    validate_metric(data, metric_x)
    validate_metric(data, metric_y)
    create_style_config(style.name, style.dark_mode)

    if size_metric:
        validate_metric(data, size_metric)

    output_path = ensure_output_directory(output_file)

    with performance_timer(f"generating scatter chart for {metric_y} vs {metric_x}"):
        metric_x_values = get_metric_values(data, metric_x)
        metric_y_values = get_metric_values(data, metric_y)
        size_values = get_metric_values(
            data, size_metric) if size_metric else None

        fig, ax = plt.subplots(figsize=style.figure_size)
        colors = cm.get_cmap('tab10')(np.linspace(0, 1, len(data)))

        for i, suite_name in enumerate(data.keys()):
            x_vals = np.array(metric_x_values[suite_name])
            y_vals = np.array(metric_y_values[suite_name])
            color = colors[i]

            if size_metric and size_values:
                sizes = np.array(size_values[suite_name]) * 10
                ax.scatter(x_vals, y_vals, s=sizes, label=suite_name,
                           alpha=0.7, color=color)
            else:
                ax.scatter(x_vals, y_vals, label=suite_name,
                           alpha=0.7, color=color)

            if trend_line and len(x_vals) > 1:
                # Vectorized trend line
                z = np.polyfit(x_vals, y_vals, 1)
                x_trend = np.linspace(np.min(x_vals), np.max(x_vals), 100)
                y_trend = np.polyval(z, x_trend)
                ax.plot(x_trend, y_trend, "--",
                        linewidth=1, color=color, alpha=0.6)

        # Styling
        chart_title = title or f'{metric_y} vs {metric_x}'
        ax.set_title(chart_title, fontsize=14, fontweight='bold')
        ax.set_xlabel(metric_x)
        ax.set_ylabel(metric_y)
        ax.legend(bbox_to_anchor=(1.05, 1), loc='upper left')
        ax.grid(True, linestyle='--', alpha=0.3)

        plt.tight_layout()
        plt.savefig(output_path, dpi=style.dpi, bbox_inches='tight')

        if show:
            plt.show()
        else:
            plt.close(fig)

    logger.info(f"Generated scatter chart: {output_path}")
    return output_path


@validate_data_integrity
def generate_pie_chart(data: PerformanceData, metric: str, output_file: Union[str, Path],
                       show: bool = False, style: Optional[ChartStyle] = None,
                       explode: bool = False, percentage: bool = True,
                       title: Optional[str] = None) -> Path:
    """Generate an optimized pie chart for a specific metric."""
    if style is None:
        style = ChartStyle()

    validate_metric(data, metric)
    create_style_config(style.name, style.dark_mode)

    output_path = ensure_output_directory(output_file)

    with performance_timer(f"generating pie chart for {metric}"):
        metric_values = get_metric_values(data, metric)

        suites = list(metric_values.keys())
        avg_values = [np.mean(values) for values in metric_values.values()]

        fig, ax = plt.subplots(figsize=(10, 10))

        explodes = tuple(0.05 for _ in suites) if explode else None
        autopct = '%1.1f%%' if percentage else None

        wedges, texts, autotexts = ax.pie(avg_values, labels=suites,
                                          autopct=autopct, explode=explodes,
                                          shadow=True, startangle=90)

        # Enhance text appearance
        for autotext in autotexts or []:
            autotext.set_color('white')
            autotext.set_fontweight('bold')

        chart_title = title or f'Distribution of {metric} by Suite'
        ax.set_title(chart_title, fontsize=14, fontweight='bold')

        plt.tight_layout()
        plt.savefig(output_path, dpi=style.dpi, bbox_inches='tight')

        if show:
            plt.show()
        else:
            plt.close(fig)

    logger.info(f"Generated pie chart: {output_path}")
    return output_path


@validate_data_integrity
def generate_histogram(data: PerformanceData, metric: str, output_file: Union[str, Path],
                       show: bool = False, style: Optional[ChartStyle] = None,
                       bins: int = 30, kde: bool = False, title: Optional[str] = None) -> Path:
    """Generate an optimized histogram for a specific metric."""
    if style is None:
        style = ChartStyle()

    validate_metric(data, metric)
    create_style_config(style.name, style.dark_mode)

    output_path = ensure_output_directory(output_file)

    with performance_timer(f"generating histogram for {metric}"):
        metric_values = get_metric_values(data, metric)

        # Efficiently collect all values
        all_values = np.concatenate(
            [values for values in metric_values.values()])

        fig, ax = plt.subplots(figsize=style.figure_size)

        # Use seaborn for better-looking histograms
        sns.histplot(all_values, bins=bins, kde=kde, ax=ax, alpha=0.8)

        chart_title = title or f'Distribution of {metric}'
        ax.set_title(chart_title, fontsize=14, fontweight='bold')
        ax.set_xlabel(metric)
        ax.set_ylabel('Count')
        ax.grid(True, linestyle='--', alpha=0.3)

        plt.tight_layout()
        plt.savefig(output_path, dpi=style.dpi, bbox_inches='tight')

        if show:
            plt.show()
        else:
            plt.close(fig)

    logger.info(f"Generated histogram: {output_path}")
    return output_path


@validate_data_integrity
def generate_heatmap(data: PerformanceData, metrics: List[str], output_file: Union[str, Path],
                     show: bool = False, style: Optional[ChartStyle] = None,
                     title: Optional[str] = None) -> Path:
    """Generate an optimized heatmap for multiple metrics across suites."""
    if style is None:
        style = ChartStyle()

    for metric in metrics:
        validate_metric(data, metric)

    create_style_config(style.name, style.dark_mode)
    output_path = ensure_output_directory(output_file)

    with performance_timer(f"generating heatmap for {len(metrics)} metrics"):
        suites = list(data.keys())

        # Efficiently build matrix using numpy
        matrix = np.zeros((len(suites), len(metrics)))

        for i, suite in enumerate(suites):
            suite_data = data[suite]
            for j, metric in enumerate(metrics):
                values = [point[metric] for point in suite_data]
                matrix[i, j] = np.mean(values)

        fig, ax = plt.subplots(figsize=(max(8, len(metrics) * 1.5),
                                        max(6, len(suites) * 0.8)))

        # Custom colormap based on style
        if style.dark_mode:
            cmap = LinearSegmentedColormap.from_list(
                "custom", ["#0d1421", "#1f4e79", "#00b4d8", "#ffd60a", "#ff8500"])
        else:
            cmap = LinearSegmentedColormap.from_list(
                "custom", ["#2d5016", "#61a5c2", "#ffd60a", "#ff8500", "#d62828"])

        # Create heatmap with enhanced styling
        im = ax.imshow(matrix, cmap=cmap, aspect='auto')

        # Add colorbar
        cbar = plt.colorbar(im, ax=ax)
        cbar.ax.tick_params(labelsize=10)

        # Set ticks and labels
        ax.set_xticks(np.arange(len(metrics)))
        ax.set_yticks(np.arange(len(suites)))
        ax.set_xticklabels(metrics, rotation=45, ha='right')
        ax.set_yticklabels(suites)

        # Add text annotations
        for i in range(len(suites)):
            for j in range(len(metrics)):
                text_color = 'white' if matrix[i, j] < np.mean(
                    matrix) else 'black'
                ax.text(j, i, f'{matrix[i, j]:.2f}', ha='center', va='center',
                        color=text_color, fontweight='bold', fontsize=9)

        chart_title = title or f'Performance Metrics Heatmap'
        ax.set_title(chart_title, fontsize=14, fontweight='bold', pad=20)

        plt.tight_layout()
        plt.savefig(output_path, dpi=style.dpi, bbox_inches='tight')

        if show:
            plt.show()
        else:
            plt.close(fig)

    logger.info(f"Generated heatmap: {output_path}")
    return output_path


def generate_all_charts_optimized(data: PerformanceData, metrics: List[str],
                                  out_dir: Union[str, Path], show: bool = False,
                                  style: Optional[ChartStyle] = None) -> Path:
    """Generate all charts for given metrics with modern optimizations."""
    if style is None:
        style = ChartStyle()

    output_dir = ensure_output_directory(out_dir)

    with performance_timer(f"generating all charts for {len(metrics)} metrics"):
        # Use concurrent execution for better performance
        with ThreadPoolExecutor(max_workers=4) as executor:
            futures = []

            for metric in metrics:
                # Submit chart generation tasks
                futures.append(executor.submit(
                    generate_bar_chart, data, metric,
                    output_dir / f'{metric}_bar.png', show, style
                ))
                futures.append(executor.submit(
                    generate_line_chart, data, metric,
                    output_dir / f'{metric}_line.png', show, style
                ))
                futures.append(executor.submit(
                    generate_pie_chart, data, metric,
                    output_dir / f'{metric}_pie.png', show, style
                ))
                futures.append(executor.submit(
                    generate_histogram, data, metric,
                    output_dir / f'{metric}_histogram.png', show, style
                ))

            # Generate scatter plots for metric pairs
            if len(metrics) >= 2:
                for i, metric_x in enumerate(metrics[:-1]):
                    for metric_y in metrics[i+1:]:
                        futures.append(executor.submit(
                            generate_scatter_chart, data, metric_x, metric_y,
                            output_dir /
                            f'{metric_y}_vs_{metric_x}_scatter.png',
                            show, style
                        ))

            # Generate heatmap
            futures.append(executor.submit(
                generate_heatmap, data, metrics,
                output_dir / 'metrics_heatmap.png', show, style
            ))

            # Wait for all tasks to complete and collect results
            completed_charts = []
            for future in tqdm(as_completed(futures), total=len(futures),
                               desc="Generating charts"):
                try:
                    result = future.result()
                    completed_charts.append(result)
                except Exception as e:
                    logger.error(f"Chart generation failed: {e}")

    logger.info(f"Generated {len(completed_charts)} charts in {output_dir}")
    return output_dir


def generate_all_charts(data: PerformanceData, metrics: List[str], out_dir: str,
                        show: bool = False, style: str = 'default',
                        dark_mode: bool = False) -> None:
    """Legacy function wrapper for backward compatibility."""
    chart_style = ChartStyle(name=style, dark_mode=dark_mode)
    generate_all_charts_optimized(data, metrics, out_dir, show, chart_style)


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
        available_metrics = get_available_metrics_uncached(data)
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
            chart_style = ChartStyle(name=args.style, dark_mode=args.dark_mode)
            generate_bar_chart(data, metric, output_file,
                               args.show, chart_style)
            print(f"Generated: {output_file}")

    if args.chart_type == "line" or args.chart_type == "all":
        for metric in args.metrics:
            output_file = os.path.join(args.out_dir, f'{metric}_line.png')
            chart_style = ChartStyle(name=args.style, dark_mode=args.dark_mode)
            generate_line_chart(data, metric, output_file, args.show,
                                chart_style, trend_line=args.trend_line)
            print(f"Generated: {output_file}")

    if args.chart_type == "scatter" or (args.chart_type == "all" and len(args.metrics) >= 2):
        if args.scatter_metrics:
            output_file = os.path.join(
                args.out_dir, f'{args.scatter_metrics[1]}_vs_{args.scatter_metrics[0]}_scatter.png')
            chart_style = ChartStyle(name=args.style, dark_mode=args.dark_mode)
            generate_scatter_chart(data, args.scatter_metrics[0], args.scatter_metrics[1],
                                   output_file, args.show, chart_style, trend_line=args.trend_line)
            print(f"Generated: {output_file}")
        else:
            for i, metric_x in enumerate(args.metrics[:-1]):
                for metric_y in args.metrics[i+1:]:
                    output_file = os.path.join(
                        args.out_dir, f'{metric_y}_vs_{metric_x}_scatter.png')
                    chart_style = ChartStyle(
                        name=args.style, dark_mode=args.dark_mode)
                    generate_scatter_chart(data, metric_x, metric_y, output_file,
                                           args.show, chart_style, trend_line=args.trend_line)
                    print(f"Generated: {output_file}")

    if args.chart_type == "pie" or args.chart_type == "all":
        for metric in args.metrics:
            output_file = os.path.join(args.out_dir, f'{metric}_pie.png')
            chart_style = ChartStyle(name=args.style, dark_mode=args.dark_mode)
            generate_pie_chart(data, metric, output_file,
                               args.show, chart_style)
            print(f"Generated: {output_file}")

    if args.chart_type == "histogram" or args.chart_type == "all":
        for metric in args.metrics:
            output_file = os.path.join(args.out_dir, f'{metric}_histogram.png')
            chart_style = ChartStyle(name=args.style, dark_mode=args.dark_mode)
            generate_histogram(data, metric, output_file,
                               args.show, chart_style)
            print(f"Generated: {output_file}")

    if args.chart_type == "heatmap" or args.chart_type == "all":
        output_file = os.path.join(args.out_dir, f'metrics_heatmap.png')
        chart_style = ChartStyle(name=args.style, dark_mode=args.dark_mode)
        generate_heatmap(data, args.metrics, output_file,
                         args.show, chart_style)
        print(f"Generated: {output_file}")


class ChartGenerator:
    """Modern Python API for direct chart generation with enhanced features."""

    def __init__(self, data: Optional[PerformanceData] = None, json_file: Optional[str] = None,
                 style: str = 'default', dark_mode: bool = False):
        """Initialize ChartGenerator with data or JSON file."""
        if data is not None:
            self.data = data
        elif json_file is not None:
            self.data = load_data(json_file)
        else:
            raise ValueError("Either data or json_file must be provided")

        self.chart_style = ChartStyle(name=style, dark_mode=dark_mode)
        self.metrics = get_available_metrics_uncached(self.data)

    def bar_chart(self, metric: str, output_file: Optional[str] = None,
                  show: bool = False, **kwargs) -> str:
        """Generate a bar chart."""
        output_file = output_file or f'{metric}_bar.png'
        result_path = generate_bar_chart(self.data, metric, output_file,
                                         show, self.chart_style, **kwargs)
        return str(result_path)

    def line_chart(self, metric: str, output_file: Optional[str] = None,
                   show: bool = False, **kwargs) -> str:
        """Generate a line chart."""
        output_file = output_file or f'{metric}_line.png'
        result_path = generate_line_chart(self.data, metric, output_file,
                                          show, self.chart_style, **kwargs)
        return str(result_path)

    def scatter_chart(self, metric_x: str, metric_y: str,
                      output_file: Optional[str] = None, show: bool = False, **kwargs) -> str:
        """Generate a scatter chart."""
        output_file = output_file or f'{metric_y}_vs_{metric_x}_scatter.png'
        result_path = generate_scatter_chart(self.data, metric_x, metric_y,
                                             output_file, show, self.chart_style, **kwargs)
        return str(result_path)

    def pie_chart(self, metric: str, output_file: Optional[str] = None,
                  show: bool = False, **kwargs) -> str:
        """Generate a pie chart."""
        output_file = output_file or f'{metric}_pie.png'
        result_path = generate_pie_chart(self.data, metric, output_file,
                                         show, self.chart_style, **kwargs)
        return str(result_path)

    def histogram(self, metric: str, output_file: Optional[str] = None,
                  show: bool = False, **kwargs) -> str:
        """Generate a histogram."""
        output_file = output_file or f'{metric}_histogram.png'
        result_path = generate_histogram(self.data, metric, output_file,
                                         show, self.chart_style, **kwargs)
        return str(result_path)

    def heatmap(self, metrics: Optional[List[str]] = None,
                output_file: Optional[str] = None, show: bool = False, **kwargs) -> str:
        """Generate a heatmap."""
        metrics = metrics or self.metrics
        output_file = output_file or 'metrics_heatmap.png'
        result_path = generate_heatmap(self.data, metrics, output_file, show,
                                       self.chart_style, **kwargs)
        return str(result_path)

    def all_charts(self, metrics: Optional[List[str]] = None,
                   out_dir: str = "charts", show: bool = False) -> str:
        """Generate all charts."""
        metrics = metrics or self.metrics
        result_path = generate_all_charts_optimized(self.data, metrics, out_dir,
                                                    show, self.chart_style)
        return str(result_path)

    def generate_report(self, metrics: Optional[List[str]] = None,
                        out_dir: str = "report") -> str:
        """Generate an HTML report."""
        metrics = metrics or self.metrics
        return generate_report(self.data, metrics, out_dir, self.chart_style.name, self.chart_style.dark_mode)


def plot_from_json(json_file: str, metrics: Optional[List[str]] = None,
                   out_dir: str = "charts", show: bool = False,
                   style: str = 'default', dark_mode: bool = False) -> None:
    """Generate all charts from JSON file with optimized performance."""
    data = load_data(json_file)
    if metrics is None:
        metrics = get_available_metrics_uncached(data)
    chart_style = ChartStyle(name=style, dark_mode=dark_mode)
    generate_all_charts_optimized(data, metrics, out_dir, show, chart_style)


if __name__ == "__main__":
    main()
