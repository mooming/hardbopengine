#!/usr/bin/env python3
"""Emit API reference pages with correct chrome, then prove they validate.

What this tool owns is the part an agent gets wrong repeatedly and that carries no information: the
sidebar module list, the `current` marker, the breadcrumb depth, the prevnext footer, and the `<main>`
wrapper. What it never owns is the prose — a
tool cannot know what `BufferInputStream::IsValidIndex` was doing wrong, so every sentence comes from a
fragment file an agent wrote after reading the header.

There is deliberately no per-page method list. Every page of a class once carried a `<ul class="methods">`
listing all of them, which on `Core/TaskSystem` measured 176 KB of the class's 427 KB — 41.3 % of the bytes
were the same 43 links copied 44 times, each link bare of the description that sits beside it in the class
page's own method table. Navigation is module page -> class page -> method page, and the class page's
method table (`Name | Signature | Link | What a caller depends on`) is the one navigation surface; the
footer's *All <Class> methods* is how a method page gets back to it.

That split is the reason this exists. Chrome retyped per page drifts: a class page written by hand lists
five of its seven methods, a method page links `../index.html` from two levels up when it sits three
levels deep, and a `class="cmt"` renders unstyled because the stylesheet declares `.cm` and never
`.cmt`. Each of those passed a reader's eye and was caught only by htmlcheck.py, after the fact. Here the
chrome is derived from the module's own index page and the
page is validated before it is kept — an invalid page is never written.

Commands

  docs_page.py class   <Module> <Class> --source <header> --summary TEXT (--sections SPEC | --body FILE)
                       [--tag TAG]...
  docs_page.py method  <Module> <Class> <page-stem> --source <path> --summary TEXT
                       (--sections SPEC | --body FILE) [--label DISPLAY] [--tag TAG]...
  docs_page.py renav   <Module> <Class>       # re-sync any method list a page still carries
  docs_page.py check   <Module>|<path> ...    # htmlcheck over what was written

`--body FILE` takes a page's prose as one HTML file — every `<h2>` section in page order, starting at the
first `<h2 id="…">`. The sidebar's *This class* / *This function* list is read back out of those headings, so
one authored file cannot disagree with the chrome indexing it, and a heading that is not plain text is
refused rather than silently stripped. `--sections SPEC` is the older route, for a page whose sections are
each already their own fragment file.

SPEC is a JSON list of sections in page order:

  [{"anchor": "signature", "heading": "Signature", "file": "signature.html"},
   {"anchor": "notes",     "heading": "Notes",     "body": "<p>...</p>"}]

`anchor` must match the authoring contract's fixed ids (`signature`, `description`, `parameters`,
`return`, `example` on method pages; `description`, `template`, `properties`, `methods`, `non-member`,
`coverage` on class pages). A fragment `file` resolves against the spec file's directory.

Exit status: 0 wrote and validated, 1 a page was rejected as invalid, 2 usage error.
"""

import ast
import json
import os
import re
import subprocess
import sys
import tempfile

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
# The validator stays with the standards that made it a gate, so page writes are checked by the same
# code that checks them at commit time rather than by a copy that can drift from it.
VALIDATOR = os.path.normpath(os.path.join(SCRIPT_DIR, '..', '..', 'hb-standards', 'scripts', 'htmlcheck.py'))
REPO_ROOT = os.path.abspath(os.path.join(SCRIPT_DIR, '..', '..', '..', '..'))
DOCS = os.path.join(REPO_ROOT, 'docs')

CLASS_SECTIONS = ['description', 'template', 'properties', 'methods', 'non-member', 'coverage']
METHOD_SECTIONS = ['signature', 'description', 'parameters', 'return', 'example']

CLASS_PAGE_SECTIONS = [
    ('description', 'Class description'),
    ('template', 'Template parameters'),
    ('properties', 'Class properties'),
    ('methods', 'Class methods'),
    ('non-member', 'Non-member helpers'),
    ('coverage', 'Coverage'),
]


def die(message, code=2):
    print('docs_page: %s' % message, file=sys.stderr)
    raise SystemExit(code)


def module_nav_head(module, title):
    """The document head plus the sidebar's module list, lifted from this module's own index page.

    Lifting rather than retyping is the point: the module list changes once per new module and every
    page must agree, and the index page is the file that change lands in.
    """
    index = os.path.join(DOCS, module, 'index.html')
    if not os.path.isfile(index):
        die('%s has no docs/%s/index.html to take chrome from' % (module, module))
    src = open(index, encoding='utf-8').read()
    nav_end = src.index('</nav>') + len('</nav>')
    head = src[:nav_end]

    modules_block = re.search(r'<h3>Modules</h3>\s*<ul>.*?</ul>', head, re.S)
    if not modules_block:
        die('%s/index.html has no <h3>Modules</h3> list' % module)
    head = head[:head.index('<h3>')] + modules_block.group(0)

    # The index page sits one level above its class pages, so every relative path it uses is one
    # directory too shallow for a page lifted into docs/<Module>/<Class>/. Rewriting `../` to `../../`
    # is the whole of the translation, and htmlcheck is what proves it was needed.
    head = re.sub(r'(href|src)="\.\./', r'\1="../../', head)

    title_tag = '<title>%s — %s module — HardBop Engine API reference</title>' % (title, module)
    if '<title>' in head:
        head = re.sub(r'<title>.*?</title>', title_tag, head, count=1, flags=re.S)
    else:
        head = head.replace('</head>', '  %s\n</head>' % title_tag)
    return head


def section_blocks(sections):
    out = []
    for anchor, heading, body in sections:
        out.append('<h2 id="%s">%s</h2>\n\n%s\n' % (anchor, heading, body.rstrip('\n')))
    return ''.join(out)


def this_page_list(sections):
    return ''.join('    <li><a href="#%s">%s</a></li>\n' % (anchor, heading) for anchor, heading, _ in sections)


def method_list_html(module, cls):
    """Every page the class owns, in header order where a prior order is known, otherwise by name."""
    folder = os.path.join(DOCS, module, cls)
    if not os.path.isdir(folder):
        return ''        # the class page is usually the first thing written, so an empty list is normal
    pages = sorted(f for f in os.listdir(folder) if f.endswith('.html') and f != 'index.html')
    existing = existing_order(module, cls)
    ordered = [p for p in existing if p in pages] + [p for p in pages if p not in existing]
    items = []
    for page in ordered:
        stem = page[:-len('.html')]
        label = page_label(module, cls, page)
        items.append('    <li><a href="%s">%s</a></li>\n' % (page, label if label else stem))
    return ''.join(items)


def page_label(module, cls, page):
    """The display label already used for this page, so renav does not restyle a page's own link text.

    The span wrapper is dropped and the entities kept verbatim: a page titled `operator&gt;&gt;` has to
    be linked as `operator&gt;&gt;`, and re-escaping text that is already escaped yields
    `operator&amp;gt;&amp;gt;` on every sidebar of the class."""
    path = os.path.join(DOCS, module, cls, page)
    src = open(path, encoding='utf-8').read()
    heading = re.search(r'<h1>(.*?)</h1>', src, re.S)
    if not heading:
        return None
    return re.sub(r'</?span[^>]*>', '', heading.group(1)).strip()


def existing_order(module, cls):
    """The method-list order already recorded on the class page, which mirrors header order."""
    index = os.path.join(DOCS, module, cls, 'index.html')
    if not os.path.isfile(index):
        return []
    src = open(index, encoding='utf-8').read()
    block = re.search(r'<ul class="methods">\s*(.*?)</ul>', src, re.S)
    if not block:
        return []
    return re.findall(r'href="([^"]+\.html)"', block.group(1))


def breadcrumb(module, cls, page=None):
    trail = ('  <a href="../../index.html">Module Index</a> /\n'
             '  <a href="../index.html">%s</a> /\n' % module)
    if page is None:
        return '<p class="breadcrumb">\n%s  %s\n</p>\n\n' % (trail, cls)
    return ('<p class="breadcrumb">\n%s  <a href="index.html">%s</a> / %s\n</p>\n\n'
            % (trail, cls, page))


def footer(cls):
    return ('\n<footer class="prevnext">\n  <span><a href="index.html">&larr; %s</a></span>\n'
            '  <span><a href="index.html#methods">All %s methods</a></span>\n</footer>\n\n'
            '</main>\n</body>\n</html>\n' % (cls, cls))


def subtitle(tags, summary):
    # A page is re-emitted far more often than it is written once, and a caller that hands back what it
    # read off an existing page hands back its tags too. Duplicates say nothing twice, so they are dropped
    # here rather than trusted to every caller.
    tags = list(dict.fromkeys(tags))
    tags_html = ''.join('  <span class="tag">%s</span>\n' % t for t in tags)
    return '<p class="subtitle">\n%s  %s\n</p>\n\n' % (tags_html, summary)


def load_sections(spec_path, expected_anchors):
    with open(spec_path, encoding='utf-8') as handle:
        spec = json.load(handle)
    if not isinstance(spec, list) or not spec:
        die('%s must hold a non-empty JSON list of sections' % spec_path)
    base = os.path.dirname(os.path.abspath(spec_path))
    sections = []
    for entry in spec:
        anchor = entry.get('anchor')
        heading = entry.get('heading')
        if not anchor or not heading:
            die('%s: every section needs anchor and heading' % spec_path)
        if anchor not in expected_anchors:
            die("%s: anchor '%s' is not one of the contract's ids %s"
                % (spec_path, anchor, ', '.join(expected_anchors)))
        if 'file' in entry:
            fragment = entry['file']
            if not os.path.isabs(fragment):
                fragment = os.path.join(base, fragment)
            body = open(fragment, encoding='utf-8').read()
        elif 'body' in entry:
            body = entry['body']
        else:
            die('%s: section %s needs a file or a body' % (spec_path, anchor))
        sections.append((anchor, heading, body))
    seen = [a for a, _, _ in sections]
    duplicated = sorted({a for a in seen if seen.count(a) > 1})
    if duplicated:
        die('%s: repeated anchor(s) %s' % (spec_path, ', '.join(duplicated)))
    return sections


def keep_if_valid(path, text):
    """Write to a temporary file, validate it, and move it into place only if htmlcheck accepts it.

    One class of finding is tolerated, and only one: a dead link whose target is another page of the
    same class that has not been authored yet. A class page always links its method pages, so validating
    a page in isolation would refuse the first page of every class on a defect that is not a defect. A
    count of what was tolerated is printed, so finishing the class is a visible obligation rather than a
    forgotten one. Every other finding — an undeclared CSS class, an unbalanced tag, a chrome path that
    points at the wrong depth, an anchor with no id behind it — rejects the write.
    """
    os.makedirs(os.path.dirname(path), exist_ok=True)
    probe = path + '.new'
    with open(probe, 'w', encoding='utf-8') as handle:
        handle.write(text)
    result = subprocess.run([sys.executable, VALIDATOR, probe],
                            capture_output=True, text=True)
    if result.returncode == 0:
        os.replace(probe, path)
        print('wrote %s (%d bytes)' % (os.path.relpath(path, REPO_ROOT), len(text)))
        return
    pending = pending_sibling_links(probe, result.stdout)
    if pending is None:
        os.remove(probe)
        print(result.stdout, end='')
        print(result.stderr, end='', file=sys.stderr)
        die('%s was rejected by htmlcheck and not written' % os.path.relpath(path, REPO_ROOT), 1)
    os.replace(probe, path)
    print('wrote %s (%d bytes), %d link(s) awaiting sibling page(s): %s'
          % (os.path.relpath(path, REPO_ROOT), len(text), len(pending), ', '.join(sorted(pending))))


def pending_sibling_links(probe_path, htmlcheck_output):
    """The dead links that point at this class's own not-yet-written pages, or None if anything else failed.

    Returns a set of link targets when every reported finding is a dead link into the page's own
    directory, and None when any finding is of another kind or points outside it.
    """
    directory = os.path.dirname(os.path.abspath(probe_path))
    reported = []
    for line in htmlcheck_output.splitlines():
        if not line.startswith(probe_path + ':') and (': ' not in line or not line.startswith(probe_path)):
            continue
        for finding in line.split(': ', 1)[1].split('; '):
            reported.append(finding)
    if not reported:
        return None
    pending = set()
    for finding in reported:
        match = re.match(r"dead link\(s\) (\[.*\])", finding)
        if not match:
            return None
        try:
            targets = ast.literal_eval(match.group(1))
        except (SyntaxError, ValueError):
            return None
        for target in targets:
            if '#' in target:
                target = target.split('#')[0]
                if not target:
                    return None        # an in-page anchor with no id is a real defect, never a pending page
            resolved = os.path.normpath(os.path.join(directory, target))
            if os.path.dirname(resolved) != directory or os.path.exists(resolved):
                return None
            pending.add(target)
    return pending


def body_sections(text, origin, expected_anchors):
    """The `(anchor, heading)` pairs a `--body` file declares, read back from its own `<h2>` headings.

    Deriving the sidebar from the prose rather than from a second declaration is the point: a spec file can
    list an anchor the body never writes, and the page then carries a link to nowhere. A heading must be plain
    text for the same reason — the sidebar has no markup to give it, so markup inside a heading is silently
    lost there and the two views of the page stop agreeing.
    """
    found = re.findall(r'<h2 id="([^"]+)">([^<]*)</h2>', text)
    if not found:
        die('%s declares no <h2 id="…">Heading</h2> section' % origin)
    anchors = [anchor for anchor, _ in found]
    unknown = [anchor for anchor in anchors if anchor not in expected_anchors]
    duplicated = sorted({anchor for anchor in anchors if anchors.count(anchor) > 1})
    if unknown or duplicated:
        die('%s: unknown anchor(s) %s, repeated anchor(s) %s; the contract allows %s'
            % (origin, unknown or 'none', duplicated or 'none', ', '.join(expected_anchors)))
    return found


def command_class(argv):
    module, cls = _require(argv, 2, 'class <Module> <Class>')
    opts = _options(argv[2:], ('source', 'summary', 'sections', 'body'), ('tag',))
    if bool(opts.get('sections')) == bool(opts.get('body')):
        die('class %s %s: give exactly one of --sections or --body' % (module, cls))
    if opts['body']:
        body_text = open(opts['body'], encoding='utf-8').read()
        pairs = body_sections(body_text, opts['body'], CLASS_SECTIONS)
        body, sidebar_list = body_text.rstrip('\n'), ''.join('    <li><a href="#%s">%s</a></li>\n' % pair
                                                            for pair in pairs)
    else:
        sections = load_sections(os.path.abspath(opts['sections']), CLASS_SECTIONS)
        body, sidebar_list = section_blocks(sections), this_page_list(sections)
    head = module_nav_head(module, cls)
    sidebar = '\n\n  <h3>This class</h3>\n  <ul>\n%s  </ul>\n</nav>\n' % sidebar_list
    title = '<h1>%s</h1>\n' % cls
    page = (head + sidebar + '\n<main class="main">\n\n'
            + breadcrumb(module, cls) + title
            + subtitle([opts['source']] + opts['tag'], opts['summary'])
            + body + footer(cls))
    keep_if_valid(os.path.join(DOCS, module, cls, 'index.html'), page)


def command_method(argv):
    module, cls, stem = _require(argv, 3, 'method <Module> <Class> <page-stem>')
    opts = _options(argv[3:], ('source', 'summary', 'sections', 'label', 'body'), ('tag',))
    if bool(opts.get('sections')) == bool(opts.get('body')):
        die('method %s %s %s: give exactly one of --sections or --body' % (module, cls, stem))
    if opts['body']:
        body_text = open(opts['body'], encoding='utf-8').read()
        pairs = body_sections(body_text, opts['body'], METHOD_SECTIONS)
        body = body_text.rstrip('\n')
    else:
        sections = load_sections(os.path.abspath(opts['sections']), METHOD_SECTIONS)
        body, pairs = section_blocks(sections), [(a, h) for a, h, _ in sections]
    sidebar_list = ''.join('    <li><a href="#%s">%s</a></li>\n' % pair for pair in pairs)
    label = opts.get('label') or stem.replace('-', ' ').capitalize()
    head = module_nav_head(module, '%s::%s' % (cls, label))
    sidebar = '\n\n  <h3>This function</h3>\n  <ul>\n%s  </ul>\n</nav>\n' % sidebar_list
    page = (head + sidebar + '\n<main class="main">\n\n'
            + breadcrumb(module, cls, label) + '<h1><span class="fn">%s</span></h1>\n' % label
            + subtitle(['member of hbe::%s' % cls, opts['source']] + opts['tag'], opts['summary'])
            + body + footer(cls))
    keep_if_valid(os.path.join(DOCS, module, cls, stem + '.html'), page)


def command_renav(argv):
    module, cls = _require(argv, 2, 'renav <Module> <Class>')
    folder = os.path.join(DOCS, module, cls)
    if not os.path.isdir(folder):
        die('docs/%s/%s does not exist' % (module, cls))
    listing = method_list_html(module, cls)
    changed = 0
    scanned = 0
    for page in sorted(f for f in os.listdir(folder) if f.endswith('.html')):
        path = os.path.join(folder, page)
        src = open(path, encoding='utf-8').read()
        block = re.search(r'(<h3>Methods</h3>\s*<ul class="methods">\s*)(.*?)(\s*</ul>)', src, re.S)
        if not block:
            scanned += 1
            continue
        if page == 'index.html':
            replacement = block.group(1) + listing.rstrip('\n') + block.group(3)
        else:
            own = re.search(r'<h3>Methods</h3>\s*<ul class="methods">\s*(.*?)</ul>', src, re.S)
            keep_first = re.search(r'<li><a href="index\.html">%s</a> methods</li>' % cls, own.group(1))
            if not keep_first:
                continue
            head_line = keep_first.group(0) + '\n'
            rest = listing
            replacement = block.group(1) + head_line + rest.rstrip('\n') + block.group(3)
        updated = src[:block.start()] + replacement + src[block.end():]
        if updated == src:
            continue
        keep_if_valid(path, updated)
        changed += 1
    if scanned:
        # "0 re-synced" from a tree that carries no list anywhere is not the same claim as "0 needed
        # changing", and a caller who cannot tell them apart will keep trusting a no-op.
        print('renav: %d page(s) scanned, %d re-synced, %d carry no method list (the class page\'s '
              'method table is the navigation surface)'
              % (scanned + changed, changed, scanned))
        return
    print('renav: %d page(s) in docs/%s/%s re-synced' % (changed, module, cls))


def command_check(argv):
    if not argv:
        die('check <Module>|<path> ...')
    targets = []
    for arg in argv:
        if os.path.exists(os.path.join(REPO_ROOT, arg)):
            targets.append(os.path.join(REPO_ROOT, arg))
        elif os.path.isdir(os.path.join(DOCS, arg)):
            for root, _dirs, files in os.walk(os.path.join(DOCS, arg)):
                targets += [os.path.join(root, f) for f in files if f.endswith('.html')]
        else:
            die('neither a page nor a documented module: %s' % arg)
    result = subprocess.run([sys.executable, VALIDATOR] + targets,
                            capture_output=True, text=True)
    print(result.stdout, end='')
    return result.returncode


def _require(argv, count, usage):
    if len(argv) < count:
        die('usage: docs_page.py %s' % usage)
    return list(argv[:count]) if count > 1 else argv[0]


def _options(argv, value_keys, flag_keys):
    opts = {key: None for key in value_keys}
    flags = {key: [] for key in flag_keys}
    index = 0
    while index < len(argv):
        token = argv[index]
        if not token.startswith('--'):
            die('unexpected argument %s' % token)
        key = token[2:]
        if key in flags:
            index += 1
            if index >= len(argv):
                die('--%s needs a value' % key)
            flags[key].append(argv[index])
        elif key in opts:
            index += 1
            if index >= len(argv):
                die('--%s needs a value' % key)
            opts[key] = argv[index]
        else:
            die('unknown option --%s' % key)
        index += 1
    for key, value in opts.items():
        # `label` is cosmetic, and --sections / --body are one route each: a required-option check that
        # demanded both would make the pair exclusive choice the caller is being asked to make impossible.
        if value is None and key not in ('label', 'sections', 'body'):
            die('--%s is required' % key)
    opts.update(flags)
    return opts


def main(argv):
    if not argv:
        print(__doc__)
        return 2
    command, rest = argv[0], argv[1:]
    handlers = {'class': command_class, 'method': command_method, 'renav': command_renav, 'check': command_check}
    if command not in handlers:
        die('unknown command %s; expected class, method, renav or check' % command)
    handlers[command](rest)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
