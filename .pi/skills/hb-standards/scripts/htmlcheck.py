#!/usr/bin/env python3
"""Validate the HTML reference: tag balance, dead links, dead anchors, undeclared CSS classes.

Extracted from `.Plans/AUTHORING_method_and_class_pages.md` section 12, where it lived as a snippet
whose first instruction was "replace the glob with your module names". As a script it can run over
the whole tree, which is the only way a page that is unlinked from its module index — present on
disk, invisible to a reader — is caught at all.

Each check is earned by a defect actually found in this tree:

  * tag balance, with HTML void elements excluded. An XML parser is not a substitute: `link` and
    `meta` are valid HTML, and parsing them as XML reports "mismatched tag" on every page.
  * dead file links — reported, never opened. An earlier revision opened each target before
    checking it, and crashed with FileNotFoundError on exactly the dead link it existed to catch.
  * cross-page anchors pointing at an id the target page does not define.
  * in-page `#anchor` links with no matching id on the same page.
  * `class="..."` values the stylesheet never declares, which render unstyled and silently.
  * `pre`/`code` blocks that do not balance, and backslash-escaped markup left behind by a
    generator that quoted its own output.

Usage: htmlcheck.py [glob ...]        default: every page under docs/
"""

import glob
import os
import re
import sys
from html.parser import HTMLParser

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
DOCS = os.path.abspath(os.path.join(SCRIPT_DIR, '..', '..', '..', '..', 'docs'))
VOID = {'area', 'base', 'br', 'col', 'embed', 'hr', 'img', 'input', 'link', 'meta', 'source',
        'track', 'wbr'}


class PageParser(HTMLParser):
    def __init__(self):
        HTMLParser.__init__(self)
        self.stack = []
        self.errors = []
        self.ids = set()
        self.hrefs = []
        self.classes = set()

    def handle_starttag(self, tag, attrs):
        values = dict(attrs)
        if 'id' in values:
            self.ids.add(values['id'])
        if 'href' in values:
            self.hrefs.append(values['href'])
        for name in values.get('class', '').split():
            self.classes.add(name)
        if tag not in VOID:
            self.stack.append((tag, self.getpos()[0]))

    def handle_endtag(self, tag):
        if tag in VOID:
            return
        if not self.stack or self.stack[-1][0] != tag:
            self.errors.append((tag, self.getpos()[0]))
        else:
            self.stack.pop()


def check_page(path, css_classes):
    """Every defect in one page, as a list of human-readable findings."""
    source = open(path, encoding='utf-8', errors='ignore').read()
    parser = PageParser()
    parser.feed(source)
    parser.close()
    findings = []
    dead, cross_anchor = [], []
    for href in parser.hrefs:
        if href.startswith(('http', 'mailto:', 'data:')):
            continue
        file_part, _, fragment = href.partition('#')
        if file_part:
            target = os.path.normpath(os.path.join(os.path.dirname(path), file_part))
            if not os.path.isfile(target):
                dead.append(href)
                continue
            if fragment and not file_part.endswith('.md') and 'id="%s"' % fragment not in open(
                    target, encoding='utf-8', errors='ignore').read():
                cross_anchor.append(href)
        if not file_part and fragment and fragment not in parser.ids:
            findings.append('in-page anchor #%s has no id on this page' % fragment)
    unstyled = sorted(parser.classes - css_classes)
    if parser.errors:
        findings.append('unbalanced tag(s) %s' % parser.errors[:3])
    if parser.stack:
        findings.append('tag(s) left open %s' % parser.stack[:3])
    if dead:
        findings.append('dead link(s) %s' % dead)
    if cross_anchor:
        findings.append('anchor(s) missing in the target page %s' % cross_anchor)
    if unstyled:
        findings.append('class(es) the stylesheet does not declare %s' % unstyled)
    if len(re.findall(r'<pre><code', source)) != len(re.findall(r'</code></pre>', source)):
        findings.append('pre/code blocks do not balance')
    contaminated = re.findall(r'<[a-zA-Z/][^>]*\\|\\"', source)
    if contaminated:
        findings.append('escaped markup %s' % contaminated[:2])
    return findings


def main():
    patterns = sys.argv[1:] or [os.path.join(DOCS, '**', '*.html')]
    stylesheet = os.path.join(DOCS, 'assets', 'hbe-docs.css')
    css_classes = set()
    if os.path.isfile(stylesheet):
        css_classes = set(re.findall(r'\.([A-Za-z][\w-]*)', open(stylesheet).read()))
    bad = 0
    seen = 0
    for pattern in patterns:
        for path in sorted(glob.glob(pattern, recursive=True)):
            seen += 1
            findings = check_page(path, css_classes)
            if findings:
                bad += 1
                print('%s: %s' % (path, '; '.join(findings)))
    if not seen:
        print('no pages matched', file=sys.stderr)
        return 2
    print('html validity: %d page(s) checked, %d with problems' % (seen, bad))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
