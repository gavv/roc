# -*- coding: utf-8 -*-

import datetime
import os
import re

def get_version():
    with open('../../src/library/include/roc/version.h') as fp:
        data = fp.read()
        m = re.search(r"""^#define ROC_VERSION_MAJOR (\d+)$""", data, re.MULTILINE)
        major = m.group(1)
        m = re.search(r"""^#define ROC_VERSION_MINOR (\d+)$""", data, re.MULTILINE)
        minor = m.group(1)
        m = re.search(r"""^#define ROC_VERSION_PATCH (\d+)$""", data, re.MULTILINE)
        patch = m.group(1)
        return (major, minor, patch)

# -- General configuration ------------------------------------------------

extensions = [
    'sphinx.ext.autodoc',
    'sphinx.ext.coverage',
    'sphinx.ext.mathjax',
    'breathe',
]

templates_path = []

source_suffix = ['.rst']
exclude_patterns = []

master_doc = 'index'

project = u'Roc Toolkit'
copyright = u'%s, Roc authors' % datetime.datetime.now().year
author = u'Roc authors'

version_tuple = get_version()

version = 'Roc Toolkit %s' % '.'.join(version_tuple[:2])
release = '%s' % '.'.join(version_tuple)

today_fmt = '%Y'

pygments_style = 'sphinx'

todo_include_todos = False

# -- Options for Breathe ----------------------------------------------

breathe_projects = { 'roc': '../../build/docs/library/xml' }

breathe_default_project = 'roc'
breathe_domain_by_extension = {'h': 'c'}

# -- Options for HTML output ----------------------------------------------

html_title = '%s %s' % (project, release)

html_theme = 'nature'

html_logo = '../images/logo80.png'

html_sidebars = {
   '**': ['globaltoc.html', 'searchbox.html'],
}

html_context = {
    'css_files': ['_static/roc.css'],
    'script_files': ['/analytics.js'],
}

html_static_path = ['_static']

html4_writer = True

# -- Options for manual page output ---------------------------------------

man_pages = [
    ('manuals/roc_send', 'roc-send', u'send real-time audio', [], 1),
    ('manuals/roc_recv', 'roc-recv', u'receive real-time audio', [], 1),
    ('manuals/roc_conv', 'roc-conv', u'convert audio', [], 1),
]
