"""Starter pytest smoke tests for the template Python package."""

from __future__ import annotations

import xbox_controller_api


class TestPythonSmoke:
    def test_import_exposes_wrapper_status(self) -> None:
        bHasWrapper_ = xbox_controller_api.HAS_WRAPPER

        assert isinstance(bHasWrapper_, bool)
        assert hasattr(xbox_controller_api, "WRAPPER_IMPORT_ERROR")
