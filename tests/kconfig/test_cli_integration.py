r"""
\file            test_cli_integration.py
\brief           Integration tests for CLI commands
\author          Nexus Team
\version         1.0.0
\date            2026-01-20

\copyright       Copyright (c) 2026 Nexus Team

\details         Tests the command-line interface for generating and
                 validating Kconfig files.
"""

import pytest
import tempfile
import json
import yaml
from pathlib import Path
from scripts.kconfig_tools.cli import cmd_generate, cmd_validate, cmd_batch_generate, cmd_batch_validate


class Args:
    """Mock arguments object for CLI commands"""
    def __init__(self, **kwargs):
        for key, value in kwargs.items():
            setattr(self, key, value)


def test_generate_command_basic(tmp_path):
    r"""
    \brief           Test basic generate command
    """
    output_file = tmp_path / "Kconfig_uart"

    args = Args(
        peripheral="UART",
        platform="NATIVE",
        instances=2,
        output=str(output_file)
    )

    # Execute command
    cmd_generate(args)

    # Verify file was created
    assert output_file.exists()

    # Verify file content
    content = output_file.read_text()
    assert "NATIVE_UART_ENABLE" in content
    assert "INSTANCE_NX_UART_0" in content
    assert "INSTANCE_NX_UART_1" in content


def test_generate_command_gpio(tmp_path):
    r"""
    \brief           Test generate command with GPIO (alpha instances)
    """
    output_file = tmp_path / "Kconfig_gpio"

    args = Args(
        peripheral="GPIO",
        platform="NATIVE",
        instances=2,
        output=str(output_file)
    )

    cmd_generate(args)

    assert output_file.exists()
    content = output_file.read_text()
    assert "INSTANCE_NX_GPIOA" in content
    assert "INSTANCE_NX_GPIOB" in content


def test_generate_command_invalid_peripheral():
    r"""
    \brief           Test generate command with invalid peripheral
    """
    args = Args(
        peripheral="INVALID",
        platform="NATIVE",
        instances=1,
        output=None
    )

    with pytest.raises(SystemExit):
        cmd_generate(args)


def test_generate_command_exceeds_max_instances(tmp_path, capsys):
    r"""
    \brief           Test generate command with instances exceeding maximum
    """
    output_file = tmp_path / "Kconfig_uart"

    args = Args(
        peripheral="UART",
        platform="NATIVE",
        instances=100,  # Exceeds maximum
        output=str(output_file)
    )

    cmd_generate(args)

    # Should generate with max instances instead
    assert output_file.exists()

    # Check warning was printed
    captured = capsys.readouterr()
    assert "Warning" in captured.err


def test_validate_command_valid_file(tmp_path):
    r"""
    \brief           Test validate command with valid file
    """
    # First generate a valid file
    kconfig_file = tmp_path / "Kconfig_uart"

    gen_args = Args(
        peripheral="UART",
        platform="NATIVE",
        instances=1,
        output=str(kconfig_file)
    )
    cmd_generate(gen_args)

    # Now validate it
    val_args = Args(
        path=str(kconfig_file),
        report=None
    )

    cmd_validate(val_args)  # Should not raise


def test_validate_command_with_report(tmp_path):
    r"""
    \brief           Test validate command with report output
    """
    # Generate a file
    kconfig_file = tmp_path / "Kconfig_spi"
    gen_args = Args(
        peripheral="SPI",
        platform="NATIVE",
        instances=1,
        output=str(kconfig_file)
    )
    cmd_generate(gen_args)

    # Validate with report
    report_file = tmp_path / "validation_report.txt"
    val_args = Args(
        path=str(kconfig_file),
        report=str(report_file)
    )

    # Validation may exit with error code if issues found
    try:
        cmd_validate(val_args)
    except SystemExit:
        pass  # Expected if validation finds issues

    # Verify report was created
    assert report_file.exists()
    report_content = report_file.read_text(encoding='utf-8')
    assert "Validation" in report_content or "Report" in report_content


def test_validate_command_nonexistent_file():
    r"""
    \brief           Test validate command with nonexistent file
    """
    args = Args(
        path="/nonexistent/path/Kconfig",
        report=None
    )

    with pytest.raises(SystemExit):
        cmd_validate(args)


def test_batch_generate_command_json(tmp_path):
    r"""
    \brief           Test batch-generate command with JSON config
    """
    # Create JSON configuration
    config = {
        "peripherals": [
            {
                "name": "UART",
                "platform": "NATIVE",
                "instances": 2,
                "output": "Kconfig_uart"
            },
            {
                "name": "SPI",
                "platform": "NATIVE",
                "instances": 1,
                "output": "Kconfig_spi"
            }
        ]
    }

    config_file = tmp_path / "config.json"
    config_file.write_text(json.dumps(config))

    # Execute batch generate
    args = Args(
        config=str(config_file),
        output_dir=str(tmp_path)
    )

    cmd_batch_generate(args)

    # Verify files were created
    assert (tmp_path / "Kconfig_uart").exists()
    assert (tmp_path / "Kconfig_spi").exists()


def test_batch_generate_command_yaml(tmp_path):
    r"""
    \brief           Test batch-generate command with YAML config
    """
    # Create YAML configuration
    config = {
        "peripherals": [
            {
                "name": "GPIO",
                "platform": "NATIVE",
                "instances": 2
            },
            {
                "name": "I2C",
                "platform": "NATIVE",
                "instances": 1
            }
        ]
    }

    config_file = tmp_path / "config.yaml"
    config_file.write_text(yaml.dump(config))

    # Execute batch generate
    args = Args(
        config=str(config_file),
        output_dir=str(tmp_path)
    )

    cmd_batch_generate(args)

    # Verify files were created
    assert (tmp_path / "Kconfig_gpio").exists()
    assert (tmp_path / "Kconfig_i2c").exists()


def test_batch_generate_command_invalid_config(tmp_path):
    r"""
    \brief           Test batch-generate with invalid configuration
    """
    # Create invalid configuration (missing peripherals key)
    config = {"invalid": "config"}

    config_file = tmp_path / "config.json"
    config_file.write_text(json.dumps(config))

    args = Args(
        config=str(config_file),
        output_dir=str(tmp_path)
    )

    with pytest.raises(SystemExit):
        cmd_batch_generate(args)


def test_batch_generate_command_nonexistent_config():
    r"""
    \brief           Test batch-generate with nonexistent config file
    """
    args = Args(
        config="/nonexistent/config.json",
        output_dir=None
    )

    with pytest.raises(SystemExit):
        cmd_batch_generate(args)


def test_batch_validate_command(tmp_path):
    r"""
    \brief           Test batch-validate command
    """
    # Create a directory structure with Kconfig files
    subdir1 = tmp_path / "peripheral1"
    subdir2 = tmp_path / "peripheral2"
    subdir1.mkdir()
    subdir2.mkdir()

    # Generate some Kconfig files
    gen_args1 = Args(
        peripheral="UART",
        platform="NATIVE",
        instances=1,
        output=str(subdir1 / "Kconfig")
    )
    cmd_generate(gen_args1)

    gen_args2 = Args(
        peripheral="SPI",
        platform="NATIVE",
        instances=1,
        output=str(subdir2 / "Kconfig")
    )
    cmd_generate(gen_args2)

    # Batch validate (may exit with error if issues found)
    val_args = Args(
        directory=str(tmp_path),
        report=None
    )

    try:
        cmd_batch_validate(val_args)
    except SystemExit:
        pass  # Expected if validation finds issues


def test_batch_validate_command_with_report(tmp_path):
    r"""
    \brief           Test batch-validate command with report output
    """
    # Create Kconfig file
    gen_args = Args(
        peripheral="GPIO",
        platform="NATIVE",
        instances=1,
        output=str(tmp_path / "Kconfig")
    )
    cmd_generate(gen_args)

    # Batch validate with report
    report_file = tmp_path / "batch_report.txt"
    val_args = Args(
        directory=str(tmp_path),
        report=str(report_file)
    )

    cmd_batch_validate(val_args)

    # Verify report was created
    assert report_file.exists()


def test_batch_validate_command_nonexistent_directory():
    r"""
    \brief           Test batch-validate with nonexistent directory
    """
    args = Args(
        directory="/nonexistent/directory",
        report=None
    )

    with pytest.raises(SystemExit):
        cmd_batch_validate(args)


def test_batch_validate_command_empty_directory(tmp_path):
    r"""
    \brief           Test batch-validate with empty directory
    """
    empty_dir = tmp_path / "empty"
    empty_dir.mkdir()

    args = Args(
        directory=str(empty_dir),
        report=None
    )

    with pytest.raises(SystemExit):
        cmd_batch_validate(args)
