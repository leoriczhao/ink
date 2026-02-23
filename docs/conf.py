# Configuration file for the Sphinx documentation builder.

project = 'ink'
copyright = '2024, ink contributors'
author = 'ink contributors'
release = '0.1.1'

extensions = [
    'breathe',
]

# Breathe configuration
breathe_projects = {
    'ink': '../docs/_build/doxygen/xml',
}
breathe_default_project = 'ink'

# Theme
html_theme = 'sphinx_rtd_theme'
html_theme_options = {
    'navigation_depth': 4,
    'collapse_navigation': False,
}

# General
exclude_patterns = ['_build', 'Thumbs.db', '.DS_Store']
templates_path = ['_templates']
html_static_path = ['_static']

# Create _static dir if it doesn't exist (avoids warning)
import os
os.makedirs(os.path.join(os.path.dirname(__file__), '_static'), exist_ok=True)
