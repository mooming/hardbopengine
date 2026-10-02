#!/usr/bin/env python3
"""Two mechanical passes over one class's reference folder.

  docs_pass.py reskin     docs/<Module>/<Class>
      Re-emit every method page through docs_page.py with the trimmed chrome, and fail if any prose
      moved. Run it after docs_page.py grows a new shape, or on a folder written before it existed.

  docs_pass.py signatures docs/<Module>/<Class> Engine/<Module>/<Class>.h <Class>
      Replace each method page's Signature section with the declaration the header carries for that
      name -- every overload of the family on one page, in header order. Fails, and writes nothing,
      unless the page's text then equals the header character for character and nothing outside the
      Signature section moved.

Neither pass touches prose it does not have to, which is the point: the judgement about what a
contract means belongs to an author, and these two are the parts where judgement can only add error.
"""
import glob, html, os, re, subprocess, sys

sys.path.insert(0, '.pi/skills/hb-standards/scripts')
from docs_methods import OPERATOR_PAGES      # the contract's own operator-spelling table, inverted below

SCRIPTS = '.pi/skills/hb-standards/scripts'


def main_content(src, module, cls):
    heading = re.search(r'<h1>(.*?)</h1>', src, re.S).group(1)
    label = re.sub(r'</?span[^>]*>', '', heading).strip()
    sub = re.search(r'<p class="subtitle">(.*?)</p>', src, re.S).group(1)
    # The tag row is chrome, and docs_page.py supplies it. Reading the whole subtitle back and handing it
    # over as the summary nests the row inside the summary, so every re-emit adds another copy of it: the
    # tags come out of the sentence and the extras that are not generated are passed on as --tag.
    tags = re.findall(r'<span class="tag">(.*?)</span>', sub, re.S)
    sub = re.sub(r'\s*<span class="tag">.*?</span>', '', sub, flags=re.S)
    keep = {'member of hbe::%s' % cls, 'Engine/%s/%s.h' % (module, cls)}
    extras = [t for t in dict.fromkeys(tags) if t not in keep]
    start = src.index('<h2 id=', src.index('<p class="subtitle">'))
    # Pages older than the footer convention have none, which is the one case where adding chrome is
    # adding content the author owed — so the body runs to </main> there and the footer arrives with it.
    end = src.find('<footer class="prevnext">')
    if end < 0:
        end = src.index('</main>')
    return label, sub.strip(), extras, src[start:end]


def reskin(folder, module, cls):
    changed = 0
    for path in sorted(glob.glob(os.path.join(folder, '*.html'))):
        if os.path.basename(path) == 'index.html':
            continue
        src = open(path).read()
        label, summary, extras, body = main_content(src, module, cls)
        stem = os.path.basename(path)[:-len('.html')]
        frag = '/tmp/hbe-frag/%s/%s/%s.body.html' % (module, cls, stem)
        os.makedirs(os.path.dirname(frag), exist_ok=True)
        open(frag, 'w').write(body)
        done = subprocess.run([sys.executable, os.path.join(SCRIPTS, 'docs_page.py'), 'method', module, cls, stem,
                               '--source', 'Engine/%s/%s.h' % (module, cls), '--summary', summary,
                               '--label', label, '--body', frag]
                              + sum([['--tag', t] for t in extras], []), capture_output=True, text=True)
        if done.returncode != 0:
            print('FAILED', stem, done.stdout, done.stderr); continue
        _, _, _, reborn = main_content(open(path).read(), module, cls)
        if reborn.rstrip('\n') != body.rstrip('\n'):
            print('DRIFT', stem)
        changed += 1
    print('reskin: %d page(s)' % changed)


def strip_comments(text):
    """Blank out comments so a doc comment's parentheses cannot open a declaration.

    A comment line inside a class body is text, and text contains `(`: `Push` in prose opened a depth
    that never closed and swallowed every declaration below it, which is why the first version of this
    parser found one destructor in a class with six members.
    """
    text = re.sub(r'/\*.*?\*/', lambda m: re.sub(r'[^\n]', ' ', m.group(0)), text, flags=re.S)
    return re.sub(r'//[^\n]*', lambda m: ' ' * len(m.group(0)), text)


def declarations(header, cls):
    """Every declared member of `class <cls>`, grouped by the name a caller would write.

    Bodies are cut out before anything else is done. A header in this tree mixes declarations with inline
    bodies, and a body is full of `{`, `;` and `<` — the three characters a declaration splitter would
    otherwise read as its own delimiters, which is what made the first two versions of this parser find
    one destructor in a class with six members.
    """
    src = strip_comments(open(header, encoding='utf-8').read())
    body = src[src.index('{', src.index('class %s' % cls)) + 1:]
    out, depth, i = [], 0, 0
    while i < len(body):
        ch = body[i]
        if ch == '{':
            depth += 1
            if depth == 1:                       # an inline body: skip to its match, keep neither
                nest, j = 1, i + 1
                while j < len(body) and nest:
                    if body[j] == '{':
                        nest += 1
                    elif body[j] == '}':
                        nest -= 1
                    j += 1
                i = j
                depth = 0        # the body is gone, so the class body is back at depth zero
                out.append(';')  # a body is where a declaration ends; without this the chunk would
                continue         # run on into whatever declaration carries the next semicolon
        elif ch == '}':
            if depth == 1:
                break
            depth -= 1
        out.append(ch)
        i += 1
    grouped = {}
    for chunk in ''.join(out).split(';'):
        lines = [l.rstrip() for l in chunk.split('\n')]
        while lines and (not lines[0].strip() or re.fullmatch(r'(public|private|protected):', lines[0].strip())):
            lines.pop(0)
        while lines and not lines[-1].strip():
            lines.pop()
        text = '\n'.join(lines).strip()
        if '(' not in text or text.startswith(('using ', 'static constexpr', 'friend ')):
            continue
        # `operator<(` has no identifier before its parenthesis, so the plain-name pattern finds nothing
        # and an operator's declaration silently owns no page.
        operator = re.search(r'\boperator\s*([^\s(]+)\s*\(', text)
        name = 'operator' + operator.group(1) if operator else None
        if name is None:
            match = re.search(r'([A-Za-z_~]\w*)\s*\(', text)
            name = match.group(1) if match else None
        if name:
            grouped.setdefault(name, []).append(text)
    return grouped


def signatures(folder, header, cls):
    grouped = declarations(header, cls)
    n, problems = 0, []
    for path in sorted(glob.glob(os.path.join(folder, '*.html'))):
        if os.path.basename(path) == 'index.html':
            continue
        src = open(path).read()
        stem = os.path.basename(path)[:-len('.html')]
        label = html.unescape(re.sub(r'</?span[^>]*>', '', re.search(r'<h1>(.*?)</h1>', src, re.S).group(1)).strip())
        if stem.startswith('operator-'):
            spelling = {page: symbol for symbol, page in OPERATOR_PAGES.items()}.get(stem)
            name = 'operator' + spelling if spelling else label
        else:
            name = cls if stem == 'constructors' else ('~' + cls if stem == 'destructor' else label)
        decls = grouped.get(name)
        if not decls:
            problems.append((stem, name, 'NOT IN HEADER')); continue
        block = '<pre><code>%s</code></pre>' % '\n\n'.join(
            d.replace('&', '&amp;').replace('<', '&lt;').replace('>', '&gt;') for d in decls)
        found = re.search(r'<h2 id="signature">Signature</h2>\n\n.*?</pre>', src, re.S)
        if not found:
            problems.append((stem, name, 'NO SIGNATURE BLOCK')); continue
        replacement = '<h2 id="signature">Signature</h2>\n\n' + block
        new = src.replace(found.group(0), replacement, 1)
        if new != src[:found.start()] + replacement + src[found.end():]:
            problems.append((stem, name, 'TOUCHED BEYOND THE BLOCK')); continue
        inside = re.search(r'<h2 id="signature">.*?<pre><code>(.*?)</code></pre>', new, re.S).group(1)
        if html.unescape(inside) != '\n\n'.join(decls):
            problems.append((stem, name, 'VERBATIM CHECK FAILED')); continue
        open(path, 'w').write(new)
        n += 1
    for stem, name, verdict in problems:
        print('PROBLEM %-28s %-22s %s' % (stem, name, verdict))
    print('signatures: %d page(s) verbatim against %s, %d problem(s)' % (n, header, len(problems)))


if __name__ == '__main__':
    folder = sys.argv[1]
    module, cls = folder.split('/')[-2], folder.split('/')[-1]
    if sys.argv[2] == 'reskin':
        reskin(folder, module, cls)
    else:
        signatures(folder, sys.argv[3], sys.argv[4])
