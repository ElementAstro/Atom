# conftest.py - pytest configuration and fixtures
# This project is licensed under the terms of the GPL3 license.
#
# Author: Max Qian
# License: GPL3

import pytest
import sys
import os
from pathlib import Path

# Add the python directory to the path so we can import atom modules
python_dir = Path(__file__).parent.parent
sys.path.insert(0, str(python_dir))

@pytest.fixture(scope="session")
def atom_modules():
    """Fixture to provide available atom modules."""
    modules = []
    try:
        import atom_algorithm
        modules.append("algorithm")
    except ImportError:
        pass
    
    try:
        import atom_connection
        modules.append("connection")
    except ImportError:
        pass
    
    try:
        import atom_error
        modules.append("error")
    except ImportError:
        pass
    
    try:
        import atom_io
        modules.append("io")
    except ImportError:
        pass
    
    try:
        import atom_search
        modules.append("search")
    except ImportError:
        pass
    
    try:
        import atom_sysinfo
        modules.append("sysinfo")
    except ImportError:
        pass
    
    try:
        import atom_type
        modules.append("type")
    except ImportError:
        pass
    
    try:
        import atom_web
        modules.append("web")
    except ImportError:
        pass
    
    return modules

@pytest.fixture
def temp_file(tmp_path):
    """Fixture to provide a temporary file."""
    temp_file = tmp_path / "test_file.txt"
    temp_file.write_text("test content")
    return str(temp_file)

@pytest.fixture
def temp_dir(tmp_path):
    """Fixture to provide a temporary directory."""
    return str(tmp_path)

@pytest.fixture(scope="session")
def test_data_dir():
    """Fixture to provide test data directory."""
    data_dir = Path(__file__).parent / "data"
    data_dir.mkdir(exist_ok=True)
    return str(data_dir)

# Pytest configuration
def pytest_configure(config):
    """Configure pytest with custom markers."""
    config.addinivalue_line(
        "markers", "slow: marks tests as slow (deselect with '-m \"not slow\"')"
    )
    config.addinivalue_line(
        "markers", "integration: marks tests as integration tests"
    )
    config.addinivalue_line(
        "markers", "benchmark: marks tests as performance benchmarks"
    )

def pytest_collection_modifyitems(config, items):
    """Modify test collection to add markers based on test names."""
    for item in items:
        # Add slow marker to tests with 'slow' in name
        if "slow" in item.name.lower():
            item.add_marker(pytest.mark.slow)
        
        # Add integration marker to tests with 'integration' in name
        if "integration" in item.name.lower():
            item.add_marker(pytest.mark.integration)
        
        # Add benchmark marker to tests with 'benchmark' in name
        if "benchmark" in item.name.lower():
            item.add_marker(pytest.mark.benchmark)

# Skip tests if modules are not available
def pytest_runtest_setup(item):
    """Skip tests if required modules are not available."""
    if hasattr(item, 'fixturenames'):
        if 'atom_modules' in item.fixturenames:
            # This test requires atom modules
            pass  # Will be handled by the fixture
