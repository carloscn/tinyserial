#!/usr/bin/env python3
"""Build the static site into _site/: one page per language, plus robots.txt and sitemap.xml.

index.html is the English page and the template. Elements carrying data-i18n (text),
data-i18n-html (markup) or data-i18n-content (a meta tag's content attribute) get their
translation from i18n/<lang>.json. English is served at /, the others at /<lang>/.
"""
import datetime
import html
import json
import pathlib
import re
import shutil

SITE = "https://tinyserial.mltz.tech"
LANGS = {  # code: (path, html lang, hreflang, og:locale)
    "en": ("/", "en", "en", "en_US"),
    "zh": ("/zh/", "zh-CN", "zh-Hans", "zh_CN"),
    "es": ("/es/", "es", "es", "es_ES"),
}

ROOT = pathlib.Path(__file__).resolve().parent
OUT = ROOT / "_site"

# Translatable elements never nest an element of their own tag name, so a lazy match
# up to the first matching close tag is enough.
ELEMENT = re.compile(r'<(\w+)(\s(?:[^>]*?\s)?data-i18n(-html)?="([^"]+)"[^>]*)>(.*?)</\1>', re.S)
META = re.compile(r'(<meta\s[^>]*?data-i18n-content="([^"]+)"[^>]*?content=")[^"]*(")')


def render(template, lang, strings):
    seen = set()

    def element(m):
        tag, attrs, is_html, key, inner = m.groups()
        seen.add(key)
        if lang == "en":
            return m.group(0)
        if key not in strings:
            raise SystemExit(f"{lang}: missing translation for {key!r}")
        value = strings[key] if is_html else html.escape(strings[key], quote=False)
        return f"<{tag}{attrs}>{value}</{tag}>"

    def meta(m):
        seen.add(m.group(2))
        if lang == "en":
            return m.group(0)
        return m.group(1) + html.escape(strings[m.group(2)]) + m.group(3)

    page = ELEMENT.sub(element, template)
    page = META.sub(meta, page)

    if lang != "en":
        unused = set(strings) - seen
        if unused:
            raise SystemExit(f"{lang}: translations not used by the page: {sorted(unused)}")

    path, html_lang, _, locale = LANGS[lang]
    links = [f'<link rel="canonical" href="{SITE}{path}">']
    links += [f'<link rel="alternate" hreflang="{h}" href="{SITE}{p}">' for p, _, h, _ in LANGS.values()]
    links += [
        f'<link rel="alternate" hreflang="x-default" href="{SITE}/">',
        f'<meta property="og:url" content="{SITE}{path}">',
        f'<meta property="og:locale" content="{locale}">',
    ]
    page = page.replace("<!-- i18n:links -->", "\n".join(links), 1)
    page = page.replace('<html lang="en">', f'<html lang="{html_lang}">', 1)
    return page


def sitemap():
    today = datetime.date.today().isoformat()
    alternates = "".join(
        f'\n    <xhtml:link rel="alternate" hreflang="{h}" href="{SITE}{p}"/>' for p, _, h, _ in LANGS.values()
    )
    urls = "".join(
        f"\n  <url>\n    <loc>{SITE}{p}</loc>\n    <lastmod>{today}</lastmod>{alternates}\n  </url>"
        for p, _, _, _ in LANGS.values()
    )
    return (
        '<?xml version="1.0" encoding="UTF-8"?>\n'
        '<urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9" '
        'xmlns:xhtml="http://www.w3.org/1999/xhtml">' + urls + "\n</urlset>\n"
    )


def main():
    template = (ROOT / "index.html").read_text(encoding="utf-8")
    if OUT.exists():
        shutil.rmtree(OUT)
    shutil.copytree(ROOT / "assets", OUT / "assets")

    for lang, (path, _, _, _) in LANGS.items():
        strings = {} if lang == "en" else json.loads((ROOT / "i18n" / f"{lang}.json").read_text(encoding="utf-8"))
        target = OUT / path.strip("/") / "index.html"
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(render(template, lang, strings), encoding="utf-8")

    (OUT / "robots.txt").write_text(f"User-agent: *\nAllow: /\n\nSitemap: {SITE}/sitemap.xml\n", encoding="utf-8")
    (OUT / "sitemap.xml").write_text(sitemap(), encoding="utf-8")
    print(f"built {OUT}")


if __name__ == "__main__":
    main()
