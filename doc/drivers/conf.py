# Configuration file for the Sphinx documentation builder.
#
# This is a self-contained Sphinx project for the downstream Realtek Bee driver
# documentation. It builds only this drivers/ directory and needs only Sphinx
# and the Read the Docs theme (see requirements.txt); it does NOT depend on the
# upstream Zephyr doc toolchain.
#
# Build (from the repository root):
#   python3 -m venv .venv && . .venv/bin/activate
#   pip install -r doc/drivers/requirements.txt
#   sphinx-build -b html doc/drivers doc/drivers/_build/html
#   # open doc/drivers/_build/html/index.html

project = "Realtek Zephyr Drivers"
author = "Realtek Semiconductor Corporation"
copyright = "2026, Realtek Semiconductor Corporation"

# index.rst (the drivers chapter hub) is the master document.
master_doc = "index"
root_doc = "index"

extensions = ["sphinx_rtd_theme"]
html_theme = "sphinx_rtd_theme"

exclude_patterns = ["_build", "Thumbs.db", ".DS_Store"]

# The driver docs use ".. code-block:: devicetree", whose Pygments lexer ships
# only with the full upstream Zephyr doc toolchain. Map it to the C lexer so the
# devicetree blocks highlight cleanly (and Sphinx does not warn) in this
# lightweight standalone build.
from sphinx.highlighting import lexers  # noqa: E402
from pygments.lexers.c_cpp import CLexer  # noqa: E402

lexers["devicetree"] = CLexer()
