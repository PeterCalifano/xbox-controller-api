"""Starter pytest smoke tests for the template Python package."""

from __future__ import annotations

import template_project


class TestPythonSmoke:
    def test_import_exposes_wrapper_status(self) -> None:
        bHasWrapper_ = template_project.HAS_WRAPPER

        assert isinstance(bHasWrapper_, bool)
        assert hasattr(template_project, "WRAPPER_IMPORT_ERROR")
