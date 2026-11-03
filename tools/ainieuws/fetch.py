#!/usr/bin/env python3
"""G2B AI-nieuws fetcher: RSS feeds -> ainieuws/data/news.json
Feeds: tech/AI nieuws + muziek-tech + NL Google News. Dedupe, NL topics, scoring.
Geen zware deps: alleen feedparser + stdlib. Vertaling = optioneel (argos) met fallback."""
import json, re, time, hashlib, html
from pathlib import Path
from datetime import datetime, timezone, timedelta
import feedparser

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "ainieuws" / "data" / "news.json"
OUT.parent.mkdir(parents=True, exist_ok=True)

UA = {"User-Agent": "Mozilla/5.0 (X11; Linux x86_64) G2B-AINieuws/1.0"}

FEEDS = [
    ("TechCrunch AI", "https://techcrunch.com/category/artificial-intelligence/feed/", 1.0, "en"),
    ("The Verge AI", "https://www.theverge.com/rss/ai-artificial-intelligence/index.xml", 1.0, "en"),
    ("Ars Technica", "https://feeds.arstechnica.com/arstechnica/technology-lab", 0.8, "en"),
    ("MIT Tech Review", "https://www.technologyreview.com/topic/artificial-intelligence/feed/", 0.9, "en"),
    ("OpenAI", "https://openai.com/news/rss.xml", 0.9, "en"),
    ("Hugging Face", "https://huggingface.co/blog/feed.xml", 0.7, "en"),
    ("Google Nieuws NL (AI)", "https://news.google.com/rss/search?q=kunstmatige+intelligentie&hl=nl&gl=NL&ceid=NL:nl", 1.0, "nl"),
    ("Google Nieuws NL (tech)", "https://news.google.com/rss/search?q=technologie+innovatie&hl=nl&gl=NL&ceid=NL:nl", 0.7, "nl"),
    ("Google Nieuws NL (muziektech)", "https://news.google.com/rss/search?q=muziek+technologie+OR+AI+muziek&hl=nl&gl=NL&ceid=NL:nl", 0.8, "nl"),
]

TOPICS = {
    "Agents": ["agent", "agents", "autonomous", "autonoom"],
    "Modellen": ["model", "llm", "gpt", "claude", "gemini", "llama", "mistral", "diffusion"],
    "Audio/Muziek": ["audio", "muziek", "music", "suno", "udio", "stem", "beat", "sound"],
    "Video": ["video", "sora", "veo", "runway", "film"],
    "GPU/Chips": ["gpu", "chip", "nvidia", "amd", "intel", "tpu", "silicon"],
    "Wetgeving": ["wet", "regul", "copyright", "auteursrecht", "eu ", "lawsuit", "recht"],
    "Tools": ["tool", "app", "release", "launch", "api", "plugin", "opensource", "open source"],
    "Onderzoek": ["research", "paper", "study", "onderzoek", "benchmark"],
}
def topic(t):
    tl = t.lower()
    for k, kws in TOPICS.items():
        if any(kw in tl for kw in kws): return k
    return "Overig"

def norm(t):
    return re.sub(r"[^a-z0-9 ]", "", t.lower()).strip()

def fingerprint(t):
    return hashlib.md5(norm(t).encode()).hexdigest()[:10]

def strip_html(s):
    return html.unescape(re.sub(r"<[^>]+>", " ", s or "")).strip()

def main():
    items = {}
    now = time.time()
    for name, url, weight, lang in FEEDS:
        try:
            d = feedparser.parse(url, request_headers=UA)
            for e in d.entries[:25]:
                title = strip_html(getattr(e, "title", ""))
                if not title: continue
                link = getattr(e, "link", "")
                summ = strip_html(getattr(e, "summary", getattr(e, "description", "")))[:300]
                # timestamp
                ts = None
                for k in ("published_parsed", "updated_parsed"):
                    v = getattr(e, k, None)
                    if v:
                        ts = time.mktime(v); break
                if not ts: ts = now
                fp = fingerprint(title)
                if fp in items:
                    items[fp]["mentions"] += 1
                    continue
                items[fp] = {
                    "id": fp, "title": title, "title_nl": None,
                    "link": link, "source": name, "lang": lang,
                    "published": datetime.fromtimestamp(ts, tz=timezone.utc).isoformat(),
                    "summary": summ, "topic": topic(title + " " + summ),
                    "mentions": 1, "ts": ts, "w": weight,
                }
        except Exception as ex:
            print("feed fail", name, ex, flush=True)
    # score: recency + weight + mentions
    for it in items.values():
        age_h = max(0.5, (now - it["ts"]) / 3600)
        it["score"] = round(3.0 / (1 + age_h / 12) + it["w"] + 2 * (it["mentions"] - 1), 3)
    out = sorted(items.values(), key=lambda x: -x["score"])
    for it in out: it.pop("ts"); it.pop("w")
    doc = {"generated": datetime.now(timezone.utc).isoformat(), "count": len(out), "items": out[:80]}
    OUT.write_text(json.dumps(doc, ensure_ascii=False, indent=1))
    print(f"news.json: {len(out)} items")

if __name__ == "__main__":
    main()
