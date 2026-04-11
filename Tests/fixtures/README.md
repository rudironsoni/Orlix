# Test Fixtures

This directory contains test fixtures for Tier A command conformance tests.

## Files

- `sample-text.txt` - Plain text file for basic tests
- `unsorted.txt` - Unsorted lines for sort command tests
- `special-chars.txt` - File with special characters
- `empty/` - Empty directory
- `tree/` - Directory tree structure for recursive tests

## Usage

Tests copy fixtures to a temporary directory before running commands.
This ensures test isolation and repeatability.
