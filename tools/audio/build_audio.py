"""Builds every audio file in assets/audio from its open-license sources, and writes CREDITS.md.

This script is the exact record of how each shipped sound was made: where its sources came
from, and every edit (trim, filter, transpose, loop, mix, loudness). Sources are downloaded into
a cache folder outside the repository the first time; after that the build works offline.

    python tools/audio/build_audio.py            # fetch missing sources, build everything
    python tools/audio/build_audio.py --only music,notes
    python tools/audio/build_audio.py --cache D:\\audio-cache

Needs Python 3.10+, numpy, scipy, soundfile (libsndfile 1.1+ for OGG/MP3), and pyloudnorm.
"""
import argparse
import fractions
import io
import json
import os
import re
import sys
import tempfile
import urllib.request
import zipfile

import numpy as np
import pyloudnorm as pyln
import soundfile as sf
from scipy import signal

RATE = 48000
REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
OUT = os.path.join(REPO, "assets", "audio")

# ---------------------------------------------------------------------------
# Sources. Every license was checked on the asset's own page.
# ---------------------------------------------------------------------------

CC0 = ("CC0 1.0", "https://creativecommons.org/publicdomain/zero/1.0/")
CCBY4 = ("CC BY 4.0", "https://creativecommons.org/licenses/by/4.0/")


def buckley(slug, title, file_url):
    return {"title": title, "creator": "Scott Buckley", "page": f"https://www.scottbuckley.com.au/library/{slug}/",
            "license": CCBY4, "url": file_url,
            "attribution": f"'{title}' by Scott Buckley - released under CC-BY 4.0. www.scottbuckley.com.au"}


def freesound(user, sound_id, title, preview):
    return {"title": title, "creator": user, "page": f"https://freesound.org/people/{user}/sounds/{sound_id}/",
            "license": CC0, "url": f"https://cdn.freesound.org/previews/{preview}-hq.ogg",
            "note": "Freesound high-quality preview (OGG) of the original upload."}


def kenney(pack, title, file):
    return {"title": f"{title} (file {file})", "creator": "Kenney (www.kenney.nl)", "page": f"https://kenney.nl/assets/{pack}",
            "license": CC0, "pack": pack, "file": file}


def vcsl(path):
    parts = path.split("/")
    name = parts[-1]
    instrument = parts[2] + (f" ({parts[3]})" if len(parts) > 4 else "")  # e.g. Vibraphone (Bowed)
    return {"title": f"Versilian Community Sample Library: {instrument}, {name}", "instrument": instrument, "file": name,
            "creator": "Versilian Studios LLC (Sam Gossner)", "page": "https://github.com/sgossner/VCSL",
            "license": CC0, "url": "https://raw.githubusercontent.com/sgossner/VCSL/master/" +
            "/".join(urllib.request.quote(p) for p in path.split("/"))}


VC = "Idiophones/Struck Idiophones/"
HARP = "Chordophones/Composite Chordophones/Concert Harp/"
CHIMES = VC + "Hand Chimes/"
VIBES = VC + "Vibraphone/"

SOURCES = {
    # Music: Scott Buckley's Creative Commons library. One composer, related keys (C major, C minor,
    # D major, C major, D minor, C major, A-flat major), piano, strings, and synth pads throughout.
    "cobalt": buckley("cobalt", "Cobalt", "http://www.scottbuckley.com.au/library/wp-content/uploads/2017/11/sb_cobalt.mp3"),
    "cobalt_nopiano": dict(buckley("cobalt", "Cobalt (no piano version)", "http://www.scottbuckley.com.au/library/wp-content/uploads/2017/11/sb_cobalt_nopiano.mp3"),
                           attribution="'Cobalt' by Scott Buckley - released under CC-BY 4.0. www.scottbuckley.com.au"),
    "hiraeth": buckley("hiraeth", "Hiraeth", "https://www.scottbuckley.com.au/library/wp-content/uploads/2020/06/sb_hiraeth.mp3"),
    "reverie": buckley("reverie", "Reverie", "https://www.scottbuckley.com.au/library/wp-content/uploads/2020/03/sb_reverie.mp3"),
    "hymn": buckley("hymn-to-the-dawn", "Hymn To The Dawn", "https://www.scottbuckley.com.au/library/wp-content/uploads/2022/11/HymnToTheDawn.mp3"),
    "solace": buckley("solace", "Solace", "https://www.scottbuckley.com.au/library/wp-content/uploads/2020/05/sb_solace.mp3"),
    "home_was_you": buckley("home-was-you", "Home Was You", "https://www.scottbuckley.com.au/library/wp-content/uploads/2026/07/HomeWasYou.mp3"),

    # Ambience field recordings (Freesound, CC0).
    "forest_evening": freesound("blaukreuz", 240588, "140809_FrybgWb_Forest_Evening_F.wav", "240/240588_623488"),
    "cave_drips": freesound("Sclolex", 177958, "Water Dripping in Cave.wav", "177/177958_985466"),
    "echo_drips": freesound("editor_adp", 162116, "Water Dripping (Large Echo)", "162/162116_2155835"),
    "lake_lapping": freesound("laughatlantic", 528067, "Water lapping on lake Pukaki,South Island,New Zealand.mp3", "528/528067_3713344"),
    "campfire": freesound("kyles", 637523, "fire small campfire crackling short +air tone.flac", "637/637523_612689"),
    "soft_crickets": freesound("Goldenboy76", 857163, "Quiet Night Atmosphere - Soft Crickets (Eagle Mountain, Utah)", "857/857163_12970089"),
    "night_chorus": freesound("NickTayloe", 825635, "Night Insects in September - Midwest US (loop)", "825/825635_11606594"),
    "wind_body": freesound("haniebal", 423314, "Wind ambience", "423/423314_1871454"),
    "pine_breeze": freesound("keweldog", 181801, "breeze.wav", "181/181801_3153523"),
    "wind_gusts": freesound("BudJillett", 109485, "Wind-Gusts-late-autumn.wav", "109/109485_1213320"),
    "robin_woodland": freesound("chris_dagorne", 563153, "Windy UK Woodland in late Winter with European Robin singing", "563/563153_3380363"),
    "cliff_sea": freesound("bruno.auzet", 525029, "sea from cliff.wav", "525/525029_11519060"),
    "harbour": freesound("Kinoton", 561815, "Genaral Habour Ambience", "561/561815_2247456"),

    # Effects (Freesound, CC0).
    "torch": freesound("ldezem", 386385, "Torch - Ignite flame on soft", "386/386385_1655965"),
    "pool_splash": freesound("swordofkings128", 398039, "water splash 2", "398/398039_7586736"),
    "puddle": freesound("Robo9418", 841834, "Small Puddle Splash", "841/841834_15176424"),
    "whoosh": freesound("ch_ase", 423799, "Little Whoosh 2", "423/423799_3982955"),
    "stone_lid": freesound("Breviceps", 457529, "Opening the sarcophagus", "457/457529_9159316"),
    "stone_slab": freesound("NahuelMartinez", 844329, "Stone Slab Door Grinding - Heavy Rock Scrape (Mono)", "844/844329_17689204"),
    "rope": freesound("vestibule-door", 664929, "rope leathery creaking.wav", "664/664929_14100561"),
    "pebbles": freesound("Vrymaa", 734669, "Stone - Throwing pebbles", "734/734669_13973196"),
    "stones": freesound("iwanPlays", 567251, "Stones Falling", "567/567251_7108319"),
    "brook": freesound("pepasystem", 819768, "Small Brook Flowing Around Rocks", "819/819768_4036966"),
    "paddles": freesound("brunoboselli", 249707, "Paddles rowing on a calm water stream", "249/249707_300738"),
    "gull_pair": freesound("Canardo55", 538015, "Herring Gull 2.wav", "538/538015_11982267"),
    "gull_call": freesound("Canardo55", 538016, "Herring Gull 1.wav", "538/538016_11982267"),
    "gull_flying": freesound("henner1964", 699979, "Seagull", "699/699979_2337062"),

    # Kenney packs (CC0).
    **{f"k_grass{i}": kenney("impact-sounds", "Impact Sounds", f"footstep_grass_00{i}.ogg") for i in range(5)},
    **{f"k_concrete{i}": kenney("impact-sounds", "Impact Sounds", f"footstep_concrete_00{i}.ogg") for i in range(5)},
    **{f"k_wood{i}": kenney("impact-sounds", "Impact Sounds", f"footstep_wood_00{i}.ogg") for i in range(5)},
    **{f"k_carpet{i}": kenney("impact-sounds", "Impact Sounds", f"footstep_carpet_00{i}.ogg") for i in range(4)},
    **{f"k_plank{i}": kenney("impact-sounds", "Impact Sounds", f"impactPlank_medium_00{i}.ogg") for i in range(5)},
    "k_soft": kenney("impact-sounds", "Impact Sounds", "impactSoft_medium_000.ogg"),
    **{f"k_thud{i}": kenney("impact-sounds", "Impact Sounds", f"impactSoft_heavy_00{i}.ogg") for i in range(3)},
    **{f"k_mining{i}": kenney("impact-sounds", "Impact Sounds", f"impactMining_00{i}.ogg") for i in range(3)},
    **{f"k_knock{i}": kenney("impact-sounds", "Impact Sounds", f"impactWood_medium_00{i}.ogg") for i in range(3)},
    **{f"k_step{i}": kenney("rpg-audio", "RPG Audio", f"footstep0{i}.ogg") for i in range(5)},
    **{f"k_cloth{i}": kenney("rpg-audio", "RPG Audio", f"cloth{i}.ogg") for i in range(1, 4)},
    "k_book_open": kenney("rpg-audio", "RPG Audio", "bookOpen.ogg"),
    "k_book_close": kenney("rpg-audio", "RPG Audio", "bookClose.ogg"),
    **{f"k_flip{i}": kenney("rpg-audio", "RPG Audio", f"bookFlip{i}.ogg") for i in range(1, 4)},
    "k_place1": kenney("rpg-audio", "RPG Audio", "bookPlace1.ogg"),
    "k_leather": kenney("rpg-audio", "RPG Audio", "handleSmallLeather.ogg"),
    "k_latch": kenney("rpg-audio", "RPG Audio", "metalLatch.ogg"),
    "k_tick": kenney("interface-sounds", "Interface Sounds", "tick_002.ogg"),
    "k_toggle": kenney("interface-sounds", "Interface Sounds", "toggle_001.ogg"),

    # Instrument samples (Versilian Community Sample Library, CC0).
    **{f"harp_{n}": vcsl(f"{HARP}KSHarp_{n}_mf{3 if n == 'C3' else 1}.wav")
       for n in ["C3", "E3", "G3", "B3", "D4", "F4", "A4", "C5", "E5", "G5", "B5", "D6", "F6", "A6"]},
    **{f"chime_{n}": vcsl(f"{CHIMES}sus_{n}_r01_main.wav")
       for n in ["C4", "D4", "E4", "F#4", "G#4", "A4", "C5", "D5", "E5", "F#5", "G#5", "A#5", "C6"]},
    **{f"vibes_{n}": vcsl(f"{VIBES}Soft Mallets/Vibes_soft_{n}_v1_rr1_Main.wav") for n in ["G3", "B3", "D4", "F4", "A4", "C5", "E5"]},
    **{f"bowed_{n}": vcsl(f"{VIBES}Bowed/Vibes_bowed_{n}_rr1_Main.wav") for n in ["G3", "D4", "A4", "E5"]},
    "marktree_slow_up": vcsl(VC + "Mark Trees/Legacy/windchimes_slowAsc1.wav"),
    "marktree_slow_down": vcsl(VC + "Mark Trees/Legacy/windchimes_slowDesc1.wav"),
    "marktree_up": vcsl(VC + "Mark Trees/Legacy/windchimes_asc1.wav"),
    "marktree_down": vcsl(VC + "Mark Trees/Legacy/windchimes_desc1.wav"),
    "marktree_random": vcsl(VC + "Mark Trees/Legacy/windchimes_random.wav"),
    "belltree": vcsl(VC + "Bell Tree/Stroke/BellTree_Stroke_1_Mid.wav"),
    "tubular_C4": vcsl(VC + "Tubular Bells 1/chimes_C4_p_rr1.wav"),
    "tubular_D3": vcsl(VC + "Tubular Bells 1/chimes_D3_ff_rr1.wav"),
    "tubular_D3_soft": vcsl(VC + "Tubular Bells 1/chimes_D3_pp_rr3.wav"),
    "tubular_D4": vcsl(VC + "Tubular Bells 1/chimes_D4_p_rr1.wav"),
    "tubular_C5": vcsl(VC + "Tubular Bells 2/TB_hit_C5_v4_1.wav"),
    "gong": vcsl(VC + "Gong 1/gong_p.wav"),
}

# ---------------------------------------------------------------------------
# Fetching
# ---------------------------------------------------------------------------


def http_get(url, referer=None):
    request = urllib.request.Request(url, headers={"User-Agent": "Mozilla/5.0 (NodeWanderer audio build)",
                                                   **({"Referer": referer} if referer else {})})
    with urllib.request.urlopen(request, timeout=120) as response:
        return response.read()


def kenney_pack(cache, pack):
    folder = os.path.join(cache, "kenney", pack)
    if not os.path.isdir(folder):
        page = http_get(f"https://kenney.nl/assets/{pack}").decode("utf-8", "replace")
        download_id = re.search(r"data-download-id='([0-9a-f]+)'", page).group(1)
        link = json.loads(http_get(f"https://kenney.nl/download-link/{download_id}", f"https://kenney.nl/assets/{pack}"))
        zipfile.ZipFile(io.BytesIO(http_get(link["url"]))).extractall(folder)
    return folder


def source_path(cache, key):
    src = SOURCES[key]
    if "pack" in src:
        folder = kenney_pack(cache, src["pack"])
        for root, _, files in os.walk(folder):
            if src["file"] in files:
                return os.path.join(root, src["file"])
        raise FileNotFoundError(src["file"])
    ext = os.path.splitext(src["url"].split("?")[0])[1]
    path = os.path.join(cache, "src", key + ext)
    if not os.path.exists(path):
        os.makedirs(os.path.dirname(path), exist_ok=True)
        print(f"  fetching {key} ...")
        data = http_get(src["url"])
        with open(path, "wb") as f:
            f.write(data)
    return path


# ---------------------------------------------------------------------------
# Signal helpers (all audio is float64, shape (frames, channels), at RATE)
# ---------------------------------------------------------------------------

CACHE = None
USES = {}      # output file -> list of (source key, edit description)
CURRENT = []   # source uses recorded while building the current output


def load(key, mono=False):
    data, rate = sf.read(source_path(CACHE, key), always_2d=True)
    if rate != RATE:
        ratio = fractions.Fraction(RATE, rate).limit_denominator(1000)
        data = signal.resample_poly(data, ratio.numerator, ratio.denominator, axis=0)
    if mono or data.shape[1] == 1:
        data = data.mean(axis=1, keepdims=True)
    return data


def use(key, edit, mono=False, start=None, end=None):
    CURRENT.append((key, edit))
    data = load(key, mono)
    if start is not None or end is not None:
        a = int((start or 0.0) * RATE)
        b = int(end * RATE) if end is not None else len(data)
        data = data[a:b]
    return data


def stereo(x):
    return x if x.shape[1] == 2 else np.repeat(x, 2, axis=1)


def to_mono(x):
    return x.mean(axis=1, keepdims=True)


def fade(x, fade_in=0.0, fade_out=0.0):
    x = x.copy()
    n_in = min(int(fade_in * RATE), len(x) // 2)
    n_out = min(int(fade_out * RATE), len(x) // 2)
    if n_in:
        x[:n_in] *= (np.sin(np.linspace(0, np.pi / 2, n_in)) ** 2)[:, None]
    if n_out:
        x[-n_out:] *= (np.cos(np.linspace(0, np.pi / 2, n_out)) ** 2)[:, None]
    return x


def db(value):
    return 10 ** (value / 20.0)


def normalized(x):
    """Peak at full scale: samples recorded at different levels can then be mixed by their gains."""
    return x / (np.max(np.abs(x)) + 1e-12)


def filt(x, kind, freq, order=2):
    sos = signal.butter(order, freq, kind, fs=RATE, output="sos")
    return signal.sosfiltfilt(sos, x, axis=0)


def lowpass(x, freq):
    return filt(x, "lowpass", freq)


def highpass(x, freq):
    return filt(x, "highpass", freq)


def repitch(x, semitones):
    """Transpose by resampling (also changes the length, as a slower or faster tape would)."""
    ratio = fractions.Fraction(2 ** (-semitones / 12.0)).limit_denominator(400)
    return signal.resample_poly(x, ratio.numerator, ratio.denominator, axis=0)


def pad_to(x, seconds):
    n = int(seconds * RATE)
    if len(x) >= n:
        return x[:n]
    return np.concatenate([x, np.zeros((n - len(x), x.shape[1]))])


def mix(length, *layers):
    """layers: (signal, offset seconds, gain dB). Output is stereo unless every layer is mono."""
    channels = 2 if any(layer[0].shape[1] == 2 for layer in layers) else 1
    out = np.zeros((int(length * RATE), channels))
    for x, offset, gain in layers:
        x = stereo(x) if channels == 2 else x
        a = int(offset * RATE)
        n = min(len(x), len(out) - a)
        if n > 0:
            out[a:a + n] += x[:n] * db(gain)
    return out


def trim_active(x, threshold_db=-50.0, pre=0.005, post=0.0):
    env = np.abs(x).max(axis=1)
    peak = env.max() + 1e-12
    on = np.where(env > peak * db(threshold_db))[0]
    if len(on) == 0:
        return x
    a = max(0, on[0] - int(pre * RATE))
    b = min(len(x), on[-1] + int(post * RATE) + 1)
    return x[a:b]


def reverb(x, seconds=1.2, wet=-12.0, damp=5000.0, predelay=0.012, seed=1):
    """A simple synthetic room: exponentially decaying stereo noise (original processing)."""
    rng = np.random.default_rng(seed)
    n = int(seconds * 1.3 * RATE)
    t = np.arange(n) / RATE
    decay = np.exp(-6.91 * t / seconds)
    ir = rng.standard_normal((n, 2)) * decay[:, None]
    ir = lowpass(ir, damp)
    ir /= np.sqrt(np.sum(ir ** 2, axis=0, keepdims=True)) + 1e-12
    ir = np.concatenate([np.zeros((int(predelay * RATE), 2)), ir])
    dry = stereo(x)
    tail = np.stack([signal.fftconvolve(dry[:, c], ir[:, c]) for c in range(2)], axis=1)
    out = np.zeros((len(tail), 2))
    out[:len(dry)] += dry
    out += tail * db(wet)
    return out


def tame_peaks(x, ceiling_db, release=0.08):
    """A gentle peak limiter: follows the peaks (5 ms hold, `release` decay) and turns down only
    what rises above `ceiling_db`, so single loud transients sit back into the bed."""
    from scipy.ndimage import maximum_filter1d
    env = maximum_filter1d(np.abs(x).max(axis=1), size=int(0.005 * RATE))
    a = np.exp(-1.0 / (release * RATE))
    env = signal.lfilter([1 - a], [1, -a], env)
    env = np.maximum(env, maximum_filter1d(np.abs(x).max(axis=1), size=int(0.002 * RATE)))
    gain = np.minimum(1.0, db(ceiling_db) / (env + 1e-12))
    return x * gain[:, None]


def momentary_max(x):
    """Loudest 400 ms window, in LUFS (short sounds are padded to 400 ms)."""
    meter = pyln.Meter(RATE, block_size=0.4)
    y = stereo(x)
    if len(y) < int(0.4 * RATE):
        y = pad_to(y, 0.4)
    best = -120.0
    hop, win = int(0.1 * RATE), int(0.4 * RATE)
    for i in range(0, max(1, len(y) - win + 1), hop):
        with np.errstate(divide="ignore"):
            value = meter.integrated_loudness(y[i:i + win])
        if np.isfinite(value):
            best = max(best, value)
    return best


def level(x, target_lufs, peak_db=-1.0):
    """Set the loudest moment to target_lufs (momentary), keeping peaks under peak_db."""
    x = x * db(target_lufs - momentary_max(x))
    peak = 20 * np.log10(np.max(np.abs(x)) + 1e-12)
    if peak > peak_db:
        x = x * db(peak_db - peak)
    return x


def trim_tail(x, floor_db=-54.0, hold=0.03, fade_len=0.06):
    """End a sound where its tail falls `floor_db` below its peak (far under the mix), with a short
    fade, so the game does not keep seconds of inaudible reverb in memory."""
    env = np.abs(x).max(axis=1)
    above = np.where(env > env.max() * db(floor_db))[0]
    if len(above) == 0:
        return x
    y = x[:min(len(x), above[-1] + int(hold * RATE))].copy()
    n = min(int(fade_len * RATE), len(y) // 2)
    if n > 0:
        y[-n:] *= (np.cos(np.linspace(0, np.pi / 2, n)) ** 2)[:, None]
    return y


def write(name, x, quality=0.55, mono=None):
    """Write assets/audio/<name>.ogg (Vorbis), recording which sources it used."""
    path = os.path.join(OUT, name + ".ogg")
    os.makedirs(os.path.dirname(path), exist_ok=True)
    if mono is True or (mono is None and x.shape[1] == 1):
        x = to_mono(x)
    if name.startswith("sfx/"):
        x = trim_tail(x)
    x = np.clip(x, -1.0, 1.0)
    with sf.SoundFile(path, "w", RATE, x.shape[1], format="OGG", subtype="VORBIS", compression_level=quality) as f:
        for i in range(0, len(x), 4096):  # libsndfile's Vorbis encoder overflows the stack on long writes
            f.write(x[i:i + 4096])
    USES[name + ".ogg"] = list(CURRENT)
    CURRENT.clear()
    return path


# ---------------------------------------------------------------------------
# Seamless loops
# ---------------------------------------------------------------------------


def seamless_loop(x, a, b, crossfade):
    """The section [a, b) whose last `crossfade` seconds blend into the audio just before a, so
    the end runs straight back into the start. Equal-power crossfade."""
    ia, ib, nf = int(a * RATE), int(b * RATE), int(crossfade * RATE)
    body = x[ia:ib].copy()
    t = np.linspace(0.0, 1.0, nf, endpoint=False)[:, None]
    body[-nf:] = x[ib - nf:ib] * np.cos(t * np.pi / 2) + x[ia - nf:ia] * np.sin(t * np.pi / 2)
    return body


def loudness_to(x, lufs, peak_db=-1.5):
    meter = pyln.Meter(RATE)
    x = x * db(lufs - meter.integrated_loudness(stereo(x)))
    peak = 20 * np.log10(np.max(np.abs(x)) + 1e-12)
    return x * db(peak_db - peak) if peak > peak_db else x


def tile(x, seconds):
    """Repeat a seamless loop to fill `seconds` (a layer under a longer bed, before the bed is looped)."""
    reps = int(np.ceil(seconds * RATE / len(x))) + 1
    return np.concatenate([x] * reps)[:int(seconds * RATE)]


def find_loop(x, start, end, search=4.0, crossfade=5.0):
    """Loop points near (start, end) whose crossfade windows sound most alike: the mean distance
    between their log spectra (64 bands), plus a penalty for different loudness."""
    mono = to_mono(x)[:, 0]
    hop, frame = int(0.05 * RATE), 4096
    window = np.hanning(frame)
    freqs = np.fft.rfftfreq(frame, 1.0 / RATE)
    band = np.digitize(freqs, np.geomspace(60, RATE / 2 - 1, 65)) - 1
    ok = (band >= 0) & (band < 64)
    count = (len(mono) - frame) // hop
    feats, loud = np.zeros((count, 64)), np.zeros(count)
    for i in range(count):
        seg = mono[i * hop:i * hop + frame]
        power = np.abs(np.fft.rfft(seg * window)) ** 2
        feats[i] = np.log10(np.bincount(band[ok], weights=power[ok], minlength=64) + 1e-12)
        loud[i] = 10 * np.log10(np.mean(seg ** 2) + 1e-12)
    w = int(crossfade / 0.05)
    best = None
    for a in range(max(w, int((start - search) / 0.05)), int((start + search) / 0.05), 2):
        for b in range(int((end - search) / 0.05), min(count - 1, int((end + search) / 0.05)), 2):
            score = -np.mean(np.abs(feats[a - w:a] - feats[b - w:b])) - 0.03 * abs(np.mean(loud[a - w:a]) - np.mean(loud[b - w:b]))
            if best is None or score > best[0]:
                best = (score, a * 0.05, b * 0.05)
    if best is None:
        raise ValueError(f"no loop points: start {start} must be at least the crossfade ({crossfade} s) after the beginning")
    return best[1], best[2]


def looped_bed(x, start, end, crossfade, search=4.0):
    a, b = find_loop(x, start, end, search, crossfade)
    return seamless_loop(x, a, b, crossfade), a, b


# ---------------------------------------------------------------------------
# Instruments: notes rendered from the nearest recorded sample, transposed by resampling
# ---------------------------------------------------------------------------

NOTE_NAMES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]


def midi(name):
    m = re.fullmatch(r"([A-G])(#|b)?(\d)", name)
    pitch = NOTE_NAMES.index(m.group(1)) + (1 if m.group(2) == "#" else -1 if m.group(2) == "b" else 0)
    return 12 * (int(m.group(3)) + 1) + pitch


def note(instrument, name, length=2.2, velocity_db=0.0):
    """Render one note. instrument: harp, chime, vibes, bowed."""
    target = midi(name)
    keys = [k for k in SOURCES if k.startswith(instrument + "_")]
    nearest = min(keys, key=lambda k: abs(midi(k.split("_", 1)[1]) - target))
    shift = target - midi(nearest.split("_", 1)[1])
    raw = load(nearest, mono=True)
    raw = raw / (np.max(np.abs(raw)) + 1e-12)  # samples were recorded at different levels
    CURRENT.append((nearest, f"note {name}" + (f" (transposed {shift:+d} semitones)" if shift else "")))
    x = repitch(trim_active(raw, -60.0, pre=0.002), shift) if shift else trim_active(raw, -60.0, pre=0.002)
    x = pad_to(x, length)
    return fade(x, 0.0, min(0.8, length * 0.4)) * db(velocity_db)


def phrase(instrument, notes, step=0.12, length=2.6, velocity_db=0.0, falloff=0.0):
    """Notes in sequence (a rolled chord or arpeggio)."""
    layers = [(note(instrument, n, length, velocity_db - falloff * i), i * step, 0.0) for i, n in enumerate(notes)]
    return mix(length + step * len(notes), *layers)


# ---------------------------------------------------------------------------
# Recipes
# ---------------------------------------------------------------------------

MUSIC = [
    # output, source, loop start, loop end, crossfade (loop points chosen by matching harmony and
    # loudness of the crossfade windows; see the loop tool notes in CREDITS.md)
    ("music/forest", "cobalt", 18.60, 179.40, 7.0),
    ("music/ruins", "hiraeth", 22.80, 166.90, 7.0),
    ("music/garden", "reverie", 8.00, 200.60, 6.0),
    ("music/edge", "cobalt_nopiano", 15.20, 179.90, 7.0),
    ("music/dawn", "hymn", 17.50, 124.50, 6.0),
    ("music/lighthouse", "solace", 22.30, 320.60, 7.0),
    ("music/harbor", "home_was_you", 75.70, 206.50, 7.0),
    ("music/finale", "hymn", 152.00, 352.20, 7.0),
]


def build_music():
    for name, key, a, b, crossfade in MUSIC:
        x = use(key, f"section {a:.2f}-{b:.2f} s, the last {crossfade:.0f} s crossfaded into the start "
                     f"(seamless loop); resampled to {RATE} Hz; loudness set to -21 LUFS")
        loop = loudness_to(seamless_loop(x, a, b, crossfade), -21.0)
        write(name, loop, quality=0.5)
        print(f"  {name}: {b - a:.1f} s")


def build_ambience():
    q = 0.65  # beds sit far under the music, so a lighter encoding is enough

    # The First Connection: an evening forest (birds, insects, wind in the trees, creaking branches).
    x = highpass(use("forest_evening", "high-pass 90 Hz; looped (5 s crossfade)"), 90)
    forest, a, b = looped_bed(x, 4.0, 82.0, 5.0)
    CURRENT[-1] = ("forest_evening", f"high-pass 90 Hz; section {a:.1f}-{b:.1f} s looped with a 5 s crossfade")
    write("ambience/forest", loudness_to(forest, -31.0), quality=q)

    # The Forgotten Return: drips in a stone hall, still water lapping far below, a faint fire.
    # The layers are mixed first and the mix is looped, so no layer can leave a seam.
    drips = lowpass(use("cave_drips", "low-pass 7 kHz; the loudest drips turned down by a gentle peak limiter"), 7000)
    drips = tame_peaks(drips / (np.max(np.abs(drips)) + 1e-12), -14.0)
    water_loop, _, _ = looped_bed(lowpass(use("lake_lapping", "low-pass 1.2 kHz, looped (3 s crossfade) and repeated under the drips, -9 dB"), 1200), 4.0, 20.5, 3.0, 1.0)
    fire_loop, _, _ = looped_bed(highpass(lowpass(use("campfire", "band-pass 400 Hz-5 kHz, looped (3 s crossfade) and repeated very low, -22 dB"), 5000), 400), 4.5, 30.5, 3.0, 1.0)
    length = len(drips) / RATE
    layered = mix(length, (drips, 0, 0.0), (tile(water_loop, length), 0, -9.0), (tile(fire_loop, length), 0, -22.0))
    ruins, a, b = looped_bed(layered, 4.0, 84.0, 4.0)
    CURRENT.append(("cave_drips", f"the mix looped: section {a:.1f}-{b:.1f} s with a 4 s crossfade"))
    write("ambience/ruins", loudness_to(ruins, -31.0), quality=q)

    # The Endless Garden: a soft night chorus of crickets (the low rumble removed), with a thin,
    # brighter chorus of katydids far behind it.
    near = highpass(use("soft_crickets", "high-pass 300 Hz"), 300)[int(40 * RATE):int(170 * RATE)]
    far_loop = highpass(lowpass(use("night_chorus", "band-pass 2-9 kHz, repeated far behind, -12 dB"), 9000), 2000)
    length = len(near) / RATE
    layered = mix(length, (near, 0, 0.0), (tile(far_loop, length), 0, -12.0))
    garden, a, b = looped_bed(layered, 10.0, 110.0, 5.0)
    CURRENT.append(("soft_crickets", f"the mix looped: section {a + 40:.1f}-{b + 40:.1f} s of the recording with a 5 s crossfade"))
    write("ambience/garden", loudness_to(garden, -33.0), quality=q)

    # The Edge of Nothing: wind across the cliffs (a deep body plus an airy breeze).
    body = use("wind_body", "the body of the wind")
    air_loop, _, _ = looped_bed(highpass(use("pine_breeze", "high-pass 1.5 kHz, looped (5 s crossfade) and repeated over the body, -4 dB"), 1500), 3.0, 60.0, 5.0)
    length = len(body) / RATE
    layered = mix(length, (body, 0, 0.0), (tile(air_loop, length), 0, -4.0))
    wind, a, b = looped_bed(layered, 6.0, 150.0, 6.0)
    CURRENT.append(("wind_body", f"the mix looped: section {a:.1f}-{b:.1f} s with a 6 s crossfade"))
    write("ambience/edge_wind", loudness_to(wind, -29.0), quality=q)

    # The ravine's deep draft, heard near the broken bridge (transposed before looping).
    deep = repitch(lowpass(use("wind_body", "low-pass 350 Hz, transposed down 5 semitones (the ravine's draft)"), 350), -5)
    gap, a, b = looped_bed(deep, 20.0, 150.0, 6.0)
    CURRENT.append(("wind_body", f"looped: section {a:.1f}-{b:.1f} s of the transposed wind with a 6 s crossfade"))
    write("ambience/edge_gap", loudness_to(gap, -31.0), quality=q)

    # Morning after the repair: a robin in a windy wood.
    x = highpass(use("robin_woodland", "high-pass 220 Hz"), 220)
    dawn, a, b = looped_bed(x, 6.0, 84.0, 5.0)
    CURRENT.append(("robin_woodland", f"section {a:.1f}-{b:.1f} s looped with a 5 s crossfade"))
    write("ambience/edge_dawn", loudness_to(dawn, -32.0), quality=q)

    # The Lighthouse Stair: waves breaking on the rocks below, heard from the cliff top, with a thin
    # breeze over them. The biggest breakers are turned down so the surf stays a bed.
    surf = highpass(use("cliff_sea", "high-pass 70 Hz; the biggest breakers turned down by a gentle peak limiter"), 70)
    surf = tame_peaks(surf / (np.max(np.abs(surf)) + 1e-12), -9.0, release=0.4)
    air_loop, _, _ = looped_bed(highpass(use("pine_breeze", "high-pass 1.5 kHz, looped (5 s crossfade) and repeated over the surf, -14 dB"), 1500), 3.0, 60.0, 5.0)
    length = len(surf) / RATE
    layered = mix(length, (surf, 0, 0.0), (tile(air_loop, length), 0, -14.0))
    cliff, a, b = looped_bed(layered, 8.0, 160.0, 6.0, 6.0)
    CURRENT.append(("cliff_sea", f"the mix looped: section {a:.1f}-{b:.1f} s with a 6 s crossfade"))
    write("ambience/lighthouse", loudness_to(cliff, -30.0), quality=q)

    # The Patient Harbor: water against the quay, boats creaking at their moorings, gulls far off.
    x = highpass(use("harbour", "high-pass 80 Hz"), 80)
    harbor, a, b = looped_bed(x, 30.0, 160.0, 6.0, 10.0)
    CURRENT.append(("harbour", f"section {a:.1f}-{b:.1f} s looped with a 6 s crossfade"))
    write("ambience/harbor", loudness_to(harbor, -31.0), quality=q)

def steps(prefix, keys, target, edit, lp=8000.0, hp=60.0, room=None, layer=None):
    for i, key in enumerate(keys, 1):
        x = trim_active(use(key, edit, mono=True), -45.0, pre=0.003, post=0.06)
        x = highpass(lowpass(x, lp), hp)
        if layer is not None:
            splash = layer(i)
            x = mix(max(len(x), len(splash)) / RATE, (x, 0, 0.0), (splash, 0.01, -9.0))
        if room:
            x = to_mono(reverb(x, room, wet=-16.0, damp=4000.0, seed=i))
        x = fade(pad_to(x, min(len(x) / RATE, 0.45)), 0.0, 0.05)
        write(f"sfx/step/{prefix}_{i}", level(x, target), quality=0.5)


def build_steps():
    steps("grass", [f"k_grass{i}" for i in range(5)], -30, "trimmed to the step, low-pass 8 kHz", lp=8000)
    steps("earth", [f"k_step{i}" for i in range(5)], -31, "trimmed, low-pass 6 kHz", lp=6000)
    steps("stone", [f"k_concrete{i}" for i in range(5)], -31, "trimmed, low-pass 7 kHz, short synthetic room added", lp=7000, room=0.35)
    puddle = load("puddle", mono=True)

    def wet_layer(i):
        CURRENT.append(("puddle", "small splash layered under the step, low-pass 6 kHz"))
        return lowpass(repitch(trim_active(puddle, -40.0), (i - 2) * 1.5), 6000)
    steps("wet", [f"k_concrete{i}" for i in range(4)], -30, "trimmed, low-pass 7 kHz, puddle splash layered, short room", lp=7000,
          room=0.45, layer=wet_layer)
    steps("wood", [f"k_wood{i}" for i in range(5)], -30, "trimmed, high-pass 70 Hz, low-pass 6 kHz", lp=6000, hp=70)
    steps("soft", [f"k_carpet{i}" for i in range(4)], -33, "trimmed, low-pass 2.5 kHz (a soft, springy cap)", lp=2500)


def build_moves():
    for i, key in enumerate(["k_cloth1", "k_cloth2", "k_cloth3"], 1):
        x = trim_active(use(key, "coat and backpack rustle, trimmed, low-pass 7 kHz", mono=True), -40.0, post=0.03)
        write(f"sfx/move/jump_{i}", level(fade(lowpass(x, 7000), 0.005, 0.08), -33), quality=0.5)
    land = trim_active(use("k_soft", "soft thud, low-pass 1.6 kHz", mono=True), -45.0, post=0.05)
    write("sfx/move/land", level(fade(lowpass(land, 1600), 0, 0.05), -29), quality=0.5)
    splash = use("pool_splash", "trimmed to 1.3 s with a fade, low-pass 7 kHz", mono=True, start=0.0, end=1.4)
    write("sfx/move/splash", level(fade(lowpass(splash, 7000), 0.002, 0.5), -27), quality=0.5)
    fall = repitch(use("whoosh", "transposed down 4 semitones, low-pass 3 kHz, faded out (falling through air)", mono=True), -4)
    write("sfx/move/fall", level(fade(lowpass(fall, 3000)[:int(1.9 * RATE)], 0.05, 0.9), -28), quality=0.5)
    gather = use("marktree_slow_up", "0-2.8 s, low-pass 9 kHz, faded (light gathering back)", start=0.0, end=2.8)
    write("sfx/move/recover", level(fade(reverb(lowpass(gather, 9000), 1.4, -14.0), 0.25, 1.2), -27), quality=0.5)


# Each place's key, for the notes of its links (from a key estimate of each music loop).
NEXT_NOTES = {  # region -> {node id: note} for arriving on that node through next
    1: {2: "D5", 3: "E5", 4: "G5", 5: "A5"},          # C major pentatonic
    2: {2: "Eb5", 3: "F5", 4: "G5", 5: "Bb5"},        # C minor pentatonic
    3: {2: "E5", 3: "F#5", 4: "A5"},                  # D major pentatonic
    4: {2: "D5", 3: "E5"},                            # C major, unresolved: the way runs out
    5: {2: "D5", 3: "F5", 4: "A5", 5: "D6"},          # D minor, rising with the stair (push); pop plays them back down
    6: {1: "C5", 2: "D5", 3: "E5", 4: "G5", 5: "A5", 6: "C6"},  # C major pentatonic, in the order the boats came
}
BACK_NOTES = {1: "C5", 2: "Eb5", 3: "F5", 4: "G5"}   # Region 2, arriving through previous (cool vibraphone)


def link_note(name, room=1.4):
    harp = note("harp", name, 2.4)
    shimmer = note("chime", name, 2.4, -14.0)
    return reverb(mix(2.4, (harp, 0, 0.0), (shimmer, 0.01, 0.0)), room, wet=-13.0)


def build_notes():
    for region, notes in NEXT_NOTES.items():
        for node, name in notes.items():
            x = link_note(name)
            write(f"sfx/notes/r{region}_n{node}", level(fade(x, 0, 0.9), -27 if region != 4 else -28), quality=0.5)
    for node, name in BACK_NOTES.items():
        x = mix(2.6, (note("vibes", name, 2.6), 0, 0.0), (note("bowed", name, 2.6, -16.0), 0.0, 0.0))
        write(f"sfx/notes/r2_back_n{node}", level(fade(reverb(x, 1.8, wet=-11.0), 0, 1.0), -28), quality=0.5)
    # Region 3: the tail's next is the head. Home again: root and fifth, with the garden's shimmer.
    home = mix(3.4, (phrase("harp", ["D5", "A5", "D6"], step=0.09, length=3.0), 0, 0.0),
               (use("marktree_down", "0.1-2.4 s, low-pass 10 kHz", start=0.1, end=2.4), 0.05, -10.0))
    write("sfx/notes/r3_n1", level(fade(reverb(lowpass(home, 10000), 1.8, wet=-12.0), 0, 1.2), -25), quality=0.5)
    # Region 4: the appended bridge node, in the dawn's key (A-flat major).
    bridge = phrase("harp", ["Ab4", "Eb5", "Ab5"], step=0.07, length=3.0)
    write("sfx/notes/r4_n4", level(fade(reverb(bridge, 1.6, wet=-12.0), 0, 1.2), -25), quality=0.5)
    # Region 6: the ship at the end of the line, the last node of the journey: a rolled C major chord.
    ship = mix(3.4, (phrase("harp", ["C5", "G5", "C6", "E6"], step=0.08, length=3.0), 0, 0.0),
               (note("chime", "C6", 3.0, -12.0), 0.1, 0.0))
    write("sfx/notes/r6_n7", level(fade(reverb(ship, 1.8, wet=-12.0), 0, 1.2), -25), quality=0.5)


def build_world():
    # The Endless Garden's current carrying the traveler home.
    water = use("brook", "2.0 s, high-pass 300 Hz, low-pass 6 kHz, faded", start=10.0, end=12.2)
    swell = use("marktree_up", "0.05-2.0 s", start=0.05, end=2.0)
    air = repitch(use("whoosh", "transposed up 2 semitones", mono=True), 2)
    passage = mix(2.4, (fade(lowpass(highpass(water, 300), 6000), 0.35, 0.8), 0, 0.0), (swell, 0.15, -6.0), (air, 0.0, -10.0))
    write("sfx/world/passage", level(fade(passage, 0.05, 0.7), -25), quality=0.5)

    # Taking the key's light: a bell tree and the key's notes.
    key = mix(3.2, (use("belltree", "full stroke"), 0, 0.0), (phrase("harp", ["G5", "C6"], step=0.16, length=2.6), 0.08, -2.0))
    write("sfx/world/key", level(fade(reverb(key, 1.8, wet=-11.0), 0, 1.0), -23), quality=0.5)

    # The brazier catching.
    flame = use("torch", "full ignition", mono=True)
    crackle = use("campfire", "4-6 s, band-pass 300 Hz-6 kHz, faded", start=4.0, end=6.0)
    brazier = mix(2.4, (flame, 0, 0.0), (fade(lowpass(highpass(crackle, 300), 6000), 0.1, 1.0), 0.25, -6.0))
    write("sfx/world/brazier", level(fade(brazier, 0, 0.6), -23), quality=0.5)

    # A checkpoint lantern lighting: a small latch and a little flame.
    latch = lowpass(use("k_latch", "lantern latch, low-pass 7 kHz", mono=True), 7000)
    small = repitch(use("torch", "transposed up 6 semitones (a small flame)", mono=True), 6)
    checkpoint = mix(1.0, (latch, 0, -6.0), (small, 0.06, 0.0))
    write("sfx/world/checkpoint", level(fade(checkpoint, 0, 0.3), -28), quality=0.5)

    # Exits opening, one for each place, in its key.
    gates = {
        1: mix(3.6, (phrase("harp", ["C5", "E5", "G5", "C6"], step=0.1, length=3.0), 0, 0.0),
               (note("chime", "G5", 3.0, -10.0), 0.3, 0.0),
               (use("marktree_slow_down", "0-2.5 s", start=0.0, end=2.5), 0.1, -12.0)),
        2: mix(3.6, (fade(lowpass(use("stone_lid", "0.4-2.4 s, low-pass 2.5 kHz (the slab sinking)", mono=True,
                                       start=0.4, end=2.4), 2500), 0.2, 0.8), 0, -4.0),
               (phrase("vibes", ["C5", "Eb5", "G5"], step=0.12, length=3.0), 0.3, -2.0),
               (note("bowed", "G3", 3.0, -10.0), 0.2, 0.0)),
        3: mix(3.6, (phrase("harp", ["D5", "F#5", "A5", "D6"], step=0.1, length=3.0), 0, 0.0),
               (use("marktree_random", "1-3.2 s (wisteria parting)", start=1.0, end=3.2), 0.05, -8.0)),
        4: mix(4.4, (repitch(use("tubular_C4", "transposed down 4 semitones to A-flat, faded", mono=True, end=4.0), -4), 0, -6.0),
               (phrase("harp", ["Ab4", "C5", "Eb5", "Ab5"], step=0.12, length=3.2), 0.15, 0.0),
               (use("marktree_slow_up", "0-3 s", start=0.0, end=3.0), 0.0, -12.0)),
        # The sea door at the foot of the tower: its latch, the old door swinging, and D minor.
        5: mix(4.4, (lowpass(use("k_latch", "door latch, low-pass 6 kHz", mono=True), 6000), 0, -8.0),
               (fade(lowpass(repitch(use("rope", "0.2-1.8 s, transposed down 3 semitones, low-pass 3 kHz (the door swinging)",
                                         mono=True, start=0.2, end=1.8), -3), 3000), 0.05, 0.6), 0.08, -8.0),
               (phrase("harp", ["D5", "F5", "A5", "D6"], step=0.11, length=3.2), 0.3, 0.0),
               (normalized(use("tubular_D4", "faded (the fog bell, far above)", mono=True, end=4.0)), 0.35, -16.0)),
        # The ship's cabin door: its latch, two strokes of the ship's bell, and C major.
        6: mix(4.4, (lowpass(use("k_latch", "door latch, low-pass 6 kHz", mono=True), 6000), 0, -8.0),
               (normalized(use("tubular_C5", "two strokes (the ship's bell)", mono=True, end=3.0)), 0.2, -12.0),
               (normalized(use("tubular_C5", "second stroke, softer", mono=True, end=3.0)), 0.55, -17.0),
               (phrase("harp", ["C5", "E5", "G5", "C6"], step=0.1, length=3.2), 0.3, 0.0),
               (use("marktree_slow_down", "0-2.5 s", start=0.0, end=2.5), 0.4, -14.0)),
    }
    for region, x in gates.items():
        write(f"sfx/world/gate_{region}", level(fade(reverb(x, 1.8, wet=-12.0), 0, 1.2), -23), quality=0.5)

    # Arriving in each place: a short motif in its key, under the place's name.
    arrivals = {
        1: phrase("harp", ["G4", "C5"], step=0.28, length=3.0),
        2: mix(3.4, (phrase("vibes", ["C5", "Eb5"], step=0.3, length=3.0), 0, 0.0), (note("bowed", "G3", 3.0, -12.0), 0, 0.0)),
        3: phrase("harp", ["A4", "D5", "F#5"], step=0.22, length=3.0),
        4: phrase("harp", ["C5", "G5"], step=0.4, length=3.0),
        5: mix(3.4, (phrase("harp", ["D5", "A5"], step=0.38, length=3.0), 0, 0.0),
               (normalized(use("tubular_D3_soft", "faded (a fog bell far off)", mono=True, end=3.0)), 0.0, -20.0)),
        6: phrase("harp", ["G4", "C5", "E5"], step=0.22, length=3.0),
    }
    for region, x in arrivals.items():
        write(f"sfx/world/arrive_{region}", level(fade(reverb(x, 2.0, wet=-10.0), 0, 1.3), -27), quality=0.5)

    # Walking through an exit: a breath of air that swells into the light.
    breath = lowpass(use("whoosh", "reversed (swells instead of fading), low-pass 6 kHz", mono=True), 6000)[::-1]
    exit_ = mix(2.0, (fade(breath, 0.3, 0.3), 0, 0.0), (use("marktree_slow_up", "0-1.6 s", start=0.0, end=1.6), 0.5, -12.0))
    write("sfx/world/exit", level(fade(exit_, 0.1, 0.5), -27), quality=0.5)

    # The journey's end, in the dawn's key.
    end = mix(5.6, (phrase("harp", ["Ab3", "Eb4", "Ab4", "C5", "Eb5", "Ab5"], step=0.13, length=4.2), 0, 0.0),
              (repitch(use("tubular_C4", "transposed down 4 semitones to A-flat", mono=True, end=5.0), -4), 0.8, -8.0),
              (note("chime", "G#5", 4.0, -8.0), 0.85, 0.0))
    write("sfx/world/complete", level(fade(reverb(end, 2.4, wet=-10.0), 0, 1.6), -22), quality=0.5)

    # The Edge of Nothing.
    gust = use("wind_gusts", "4-8.5 s (one gust), low-pass 5 kHz, faded", start=4.0, end=8.5)
    creak = use("rope", "0-2.2 s, low-pass 5 kHz", mono=True, start=0.0, end=2.2)
    edge = mix(4.6, (fade(lowpass(gust, 5000), 0.6, 1.6), 0, 0.0), (lowpass(creak, 5000), 1.0, -6.0))
    write("sfx/world/edge_gust", level(fade(edge, 0.1, 0.8), -26), quality=0.5)

    void = repitch(use("gong", "0-5 s, transposed down 4 semitones, low-pass 900 Hz, slow fade-in so it never strikes",
                       mono=True, end=5.0), -4)
    down = repitch(use("whoosh", "transposed down 7 semitones, low-pass 2.5 kHz", mono=True), -7)
    hazard = mix(5.0, (fade(lowpass(void, 900), 0.45, 2.5), 0.15, 0.0), (fade(lowpass(down, 2500), 0.05, 1.2), 0, -3.0))
    write("sfx/world/hazard", level(fade(hazard, 0, 1.0), -26), quality=0.5)

    repair = mix(4.4, (use("marktree_slow_up", "0-3.6 s", start=0.0, end=3.6), 0, -4.0),
                 (fade(note("bowed", "Ab4", 4.0), 0.6, 1.5), 0.0, 0.0),
                 (note("harp", "Ab4", 3.0, -4.0), 0.7, 0.0))
    write("sfx/world/repair", level(fade(reverb(repair, 2.0, wet=-11.0), 0.05, 1.2), -24), quality=0.5)

    for i in range(5):
        plank = trim_active(use(f"k_plank{i}", "trimmed, low-pass 4 kHz", mono=True), -45.0, post=0.05)
        write(f"sfx/world/plank_{i + 1}", level(fade(lowpass(plank, 4000), 0, 0.05), -28), quality=0.5)
    pier = use("stone_slab", "3-4.6 s, low-pass 1.2 kHz, faded (the pier rising from the mist)", mono=True, start=3.0, end=4.6)
    write("sfx/world/pier", level(fade(lowpass(pier, 1200), 0.3, 0.5), -30), quality=0.5)
    rope = use("rope", "2.4-3.6 s, low-pass 5 kHz (the rails drawn tight)", mono=True, start=2.4, end=3.6)
    write("sfx/world/rope", level(fade(lowpass(rope, 5000), 0.05, 0.3), -29), quality=0.5)
    for i, name in enumerate(["C5", "Eb5", "Ab5"], 1):  # lanterns along the bridge, lit in turn
        flame = repitch(use("torch", "transposed up 7 semitones", mono=True), 7)
        x = mix(2.6, (link_note(name), 0, 0.0), (flame, 0, -8.0))
        write(f"sfx/world/lantern_{i}", level(fade(x, 0, 1.0), -27), quality=0.5)
    build_stair()
    build_harbor()


def scrape(start, length, edit, semitones):
    """A short stretch of the slab's grinding, made smaller (higher) and band-limited."""
    x = use("stone_slab", f"{start:.2f}-{start + length:.2f} s, transposed {semitones:+d} semitones, band-pass 150 Hz-4 kHz ({edit})",
            mono=True, start=start, end=start + length)
    return fade(highpass(lowpass(repitch(x, semitones), 4000), 150), 0.04, 0.12)


def build_stair():
    # push(): a stone grinds out of its socket in the tower wall and locks with a thud.
    for i, (start, knock) in enumerate([(5.25, "k_thud0"), (6.05, "k_thud1")], 1):
        thud = lowpass(trim_active(use(knock, "soft heavy thud, low-pass 1.2 kHz", mono=True), -45.0, post=0.05), 1200)
        x = mix(1.0, (normalized(scrape(start, 0.42, "a stone sliding out", 3)), 0, 0.0), (normalized(thud), 0.32, -7.0))
        write(f"sfx/world/stone_push_{i}", level(fade(x, 0, 0.12), -28), quality=0.5)
    # pop(): the top stone grinds back into the wall, lower, and seats with a dull knock.
    for i, (start, knock) in enumerate([(0.10, "k_mining0"), (2.10, "k_mining1")], 1):
        seat = lowpass(repitch(trim_active(use(knock, "transposed down 5 semitones, low-pass 1.5 kHz (the stone seating)", mono=True),
                                           -45.0, post=0.05), -5), 1500)
        x = mix(1.0, (normalized(scrape(start, 0.38, "the stone sliding in", -1)), 0, 0.0), (normalized(seat), 0.34, -6.0))
        write(f"sfx/world/stone_pop_{i}", level(fade(x, 0, 0.12), -28), quality=0.5)
    # The bottom stone will not move: a dull knock, and grit trickling.
    knock = lowpass(repitch(trim_active(use("k_mining2", "transposed down 6 semitones, low-pass 1.4 kHz", mono=True), -45.0, post=0.05), -6), 1400)
    thud = lowpass(trim_active(use("k_thud2", "low-pass 900 Hz", mono=True), -45.0, post=0.05), 900)
    grit = use("pebbles", "1.2-1.8 s, low-pass 3 kHz (grit trickling)", mono=True, start=1.2, end=1.8)
    stuck = mix(0.9, (knock, 0, 0.0), (thud, 0.0, -3.0), (fade(lowpass(grit, 3000), 0.05, 0.3), 0.08, -14.0))
    write("sfx/world/stone_stuck", level(fade(stuck, 0, 0.1), -29), quality=0.5)
    # The fog bell at the top of the tower: a deep stroke, a softer second, and the fog around them.
    strike = normalized(use("tubular_D3", "full stroke (D)", mono=True))
    second = normalized(use("tubular_D3_soft", "softer second stroke", mono=True))
    shine = normalized(use("tubular_D4", "an octave above, under the stroke", mono=True))
    body = normalized(lowpass(repitch(use("gong", "0-6 s, transposed down 2 semitones, low-pass 700 Hz, under the bell", mono=True, end=6.0), -2), 700))
    bell = mix(7.0, (strike, 0, 0.0), (shine, 0.0, -10.0), (fade(body, 0.02, 3.0), 0.0, -14.0), (second, 1.9, -4.0))
    write("sfx/world/bell", level(fade(reverb(bell, 3.2, wet=-9.0, damp=4500.0), 0, 2.5), -23), quality=0.5)


def stroke(start, edit, lp=6000.0):
    """One stroke of a paddle in calm water."""
    x = use("paddles", f"{start:.2f}-{start + 0.7:.2f} s, low-pass {lp / 1000:g} kHz ({edit})", mono=True, start=start, end=start + 0.7)
    return fade(lowpass(x, lp), 0.01, 0.25)


def build_harbor():
    # enqueue(): a boat rows in to the end of the line and bumps against its post.
    knock = lowpass(trim_active(use("k_knock0", "wooden knock, low-pass 2.5 kHz (the hull against the post)", mono=True), -45.0, post=0.05), 2500)
    slap = lowpass(trim_active(use("puddle", "low-pass 2.5 kHz (water slapping the hull)", mono=True), -40.0), 2500)
    arrive = mix(2.0, (normalized(stroke(16.95, "a stroke")), 0, -3.0), (normalized(stroke(18.80, "a second stroke")), 0.5, -1.0),
                 (normalized(knock), 0.92, -2.0), (normalized(slap), 0.95, -16.0))
    write("sfx/world/boat_arrive", level(fade(arrive, 0, 0.4), -27), quality=0.5)
    # dequeue(): the front boat rows away, each stroke farther off.
    leave = mix(2.6, (stroke(20.70, "a stroke", 6000), 0, 0.0), (stroke(24.33, "farther", 3500), 0.65, -5.0),
                (stroke(26.80, "farther still", 2200), 1.3, -10.0))
    write("sfx/world/boat_leave", level(fade(reverb(leave, 1.2, wet=-16.0), 0, 0.6), -28), quality=0.5)


def build_journal():
    def clean(key, edit, lp, start=None, end=None, threshold=-45.0):
        x = use(key, edit, mono=True, start=start, end=end)
        return fade(lowpass(trim_active(x, threshold, post=0.04), lp), 0.003, 0.04)
    opening = mix(0.5, (clean("k_book_open", "trimmed, low-pass 8 kHz", 8000), 0, 0.0), (clean("k_leather", "leather strap, low-pass 6 kHz", 6000), 0.0, -6.0))
    write("sfx/journal/open", level(opening, -30), quality=0.5)
    write("sfx/journal/close", level(clean("k_book_close", "trimmed, low-pass 6 kHz", 6000), -30), quality=0.5)
    for i in range(1, 4):
        write(f"sfx/journal/page_{i}", level(clean(f"k_flip{i}", "trimmed to the flip itself, low-pass 9 kHz", 9000, threshold=-24.0), -31), quality=0.5)
    write("sfx/journal/note", level(clean("k_flip3", "trimmed, low-pass 6 kHz (a note pinned up)", 6000), -30), quality=0.5)
    write("sfx/journal/dismiss", level(clean("k_place1", "trimmed, low-pass 4 kHz (a note set down)", 4000), -32), quality=0.5)
    slip = repitch(clean("k_flip2", "transposed up 2 semitones, high-pass 900 Hz (a slip of paper)", 9000), 2)
    write("sfx/journal/callout", level(highpass(slip, 900), -34), quality=0.5)
    cover = repitch(clean("k_book_close", "transposed down 3 semitones, low-pass 1.8 kHz (the leather cover)", 1800), -3)
    write("sfx/journal/cover_close", level(cover, -30), quality=0.5)
    cover_open = repitch(clean("k_book_open", "transposed down 2 semitones, low-pass 3 kHz", 3000), -2)
    write("sfx/journal/cover_open", level(cover_open, -31), quality=0.5)
    write("sfx/journal/tick", level(clean("k_tick", "low-pass 5 kHz", 5000), -34), quality=0.5)
    write("sfx/journal/toggle", level(clean("k_toggle", "low-pass 4 kHz", 4000), -32), quality=0.5)


def isolated_events(x, count, gap=1.6, length=1.8):
    """The strongest onsets that have nothing else loud in the following `gap` seconds."""
    env = np.abs(to_mono(x))[:, 0]
    hop = int(0.01 * RATE)
    frames = np.array([env[i:i + hop].max() for i in range(0, len(env) - hop, hop)])
    order = np.argsort(frames)[::-1]
    chosen = []
    for f in order:
        t = f * 0.01
        if t < 0.2 or t + length > len(env) / RATE:
            continue
        if any(abs(t - c) < length for c in chosen):
            continue
        after = frames[f + 15:f + int(gap / 0.01)]
        if len(after) and after.max() > frames[f] * 0.35:
            continue
        chosen.append(t)
        if len(chosen) == count:
            break
    return sorted(chosen)


def build_accents():
    drips = use("echo_drips", "single drips with their echoes cut out, low-pass 8 kHz")
    CURRENT.clear()
    for i, t in enumerate(isolated_events(drips, 4), 1):
        x = use("echo_drips", f"one drip at {t:.2f} s with its echo, low-pass 8 kHz", start=t - 0.03, end=t + 1.8)
        write(f"sfx/accent/drip_{i}", level(fade(lowpass(x, 8000), 0.01, 0.8), -31), quality=0.5)
    for i, name in enumerate(["A5", "D6", "F#5"], 1):  # D major, for the garden
        x = mix(4.0, (note("chime", name, 3.8, 0.0), 0, 0.0))
        write(f"sfx/accent/chime_{i}", level(fade(reverb(x, 2.4, wet=-8.0), 0.0, 1.6), -32), quality=0.5)
    x = use("marktree_random", "4-6.5 s, low-pass 9 kHz", start=4.0, end=6.5)
    write("sfx/accent/chime_4", level(fade(reverb(lowpass(x, 9000), 2.0, wet=-10.0), 0.2, 1.0), -33), quality=0.5)
    for i, (a, b) in enumerate([(0.0, 1.7), (1.8, 3.5), (3.6, 5.4)], 1):
        x = use("rope", f"{a}-{b} s, low-pass 4.5 kHz", mono=True, start=a, end=b)
        write(f"sfx/accent/creak_{i}", level(fade(lowpass(x, 4500), 0.05, 0.4), -32), quality=0.5)
    x = use("pebbles", "1.0-2.8 s, low-pass 4 kHz, distant room", mono=True, start=1.0, end=2.8)
    write("sfx/accent/pebbles_1", level(fade(reverb(lowpass(x, 4000), 1.6, wet=-6.0), 0.02, 0.8), -33), quality=0.5)
    x = use("stones", "0.3-2.6 s, low-pass 2.5 kHz, distant room", mono=True, start=0.3, end=2.6)
    write("sfx/accent/pebbles_2", level(fade(reverb(lowpass(x, 2500), 1.8, wet=-6.0), 0.05, 1.0), -34), quality=0.5)
    # Gulls far off over the water (for the lighthouse and the harbor).
    for i, (key, a, b) in enumerate([("gull_pair", 0.20, 3.60), ("gull_flying", 0.50, 4.00), ("gull_call", 8.50, 10.40)], 1):
        x = use(key, f"{a:.1f}-{b:.1f} s, band-pass 500 Hz-5 kHz, distant room", mono=True, start=a, end=b)
        write(f"sfx/accent/gull_{i}", level(fade(reverb(highpass(lowpass(x, 5000), 500), 1.6, wet=-7.0), 0.03, 0.6), -33), quality=0.5)


# ---------------------------------------------------------------------------
# Credits
# ---------------------------------------------------------------------------

CREDITS_HEADER = """# Audio credits

Every sound and piece of music in this folder comes from an open-license source, edited for the game.
Licenses were checked on each asset's own page. The edits are listed per file below, and
`tools/audio/build_audio.py` reproduces every file from its sources.

- **Music:** six pieces by Scott Buckley, under the Creative Commons Attribution 4.0 license (CC BY 4.0). Attribution is required.
  The game also credits him on the final journal page.
- **Everything else:** public domain (CC0 1.0). No attribution is required; the creators are credited here anyway.
  Freesound recordings were taken from the site's high-quality Ogg preview of each upload.
- **Original arrangements:** the link notes, the chords of the gates and the journey's end, the arrival motifs, the
  bridge's lanterns, and the fog bell's strokes were composed for this game and played with the CC0 instrument samples listed below.
- Copies of the licenses are in `licenses/`.

Edits common to all files: converted to {rate} Hz, encoded as Ogg Vorbis, and set to a loudness that fits the mix.
Music and ambience were cut into seamless loops: a section whose last seconds crossfade into the audio just before
its start, so it plays on without a seam. "Room" means a short synthetic reverb (original processing). Effects end where their tail falls 54 dB below their peak.

## Music (CC BY 4.0)

{music}

## Sources

| Source | Creator | License |
| --- | --- | --- |
{sources}

## Files

{files}
"""


def write_credits():
    used = {}
    for out, uses in sorted(USES.items()):
        for key, edit in uses:
            used.setdefault(key, []).append((out, edit))
    music_lines = []
    for key in ["cobalt", "cobalt_nopiano", "hiraeth", "reverie", "solace", "home_was_you", "hymn"]:
        if key in used:
            src = SOURCES[key]
            files = ", ".join(sorted({f"`{o}`" for o, _ in used[key]}))
            music_lines.append(f"- {src['attribution']}  \n  Source: {src['page']} ({src['url']}). Used in {files}.")
    # One row per recording; Kenney files grouped by pack and samples grouped by instrument.
    groups = {}
    for key in used:
        src = SOURCES[key]
        if "pack" in src:
            label, item = src["title"].split(" (file ")[0], src["file"]
        elif "instrument" in src:
            label, item = "Versilian Community Sample Library: " + src["instrument"], src["file"]
        else:
            label, item = src["title"], None
        group = groups.setdefault((src["creator"], label, src["page"], src["license"]), [])
        if item:
            group.append(item)
    rows = []
    for (creator, label, page, (name, link)), items in sorted(groups.items(), key=lambda g: (g[0][0].lower(), g[0][1].lower())):
        files = f" ({', '.join(sorted(items))})" if items else ""
        rows.append(f"| [{label.replace('|', '/')}]({page}){files} | {creator} | [{name}]({link}) |")
    file_lines = []
    for out, uses in sorted(USES.items()):
        parts = []
        for key, edit in uses:
            src = SOURCES[key]
            parts.append(f"{src['title']} by {src['creator']} ({src['license'][0]}): {edit}")
        file_lines.append(f"- `{out}`  \n  " + "  \n  ".join(parts))
    text = CREDITS_HEADER.format(rate=RATE, music="\n".join(music_lines), sources="\n".join(rows), files="\n".join(file_lines))
    with open(os.path.join(OUT, "CREDITS.md"), "w", encoding="utf-8", newline="\n") as f:
        f.write(text)


# ---------------------------------------------------------------------------

STEPS = {"music": build_music, "ambience": build_ambience, "steps": build_steps, "moves": build_moves,
         "notes": build_notes, "world": build_world, "journal": build_journal, "accents": build_accents}


def main():
    global CACHE
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--cache", default=os.path.join(tempfile.gettempdir(), "nw-audio", "cache"))
    parser.add_argument("--only", default="", help="comma-separated parts: " + ", ".join(STEPS))
    args = parser.parse_args()
    CACHE = args.cache
    parts = [p for p in args.only.split(",") if p] or list(STEPS)
    manifest = os.path.join(os.path.dirname(os.path.abspath(__file__)), "uses.json")  # sources per file, so partial builds keep full credits
    if os.path.exists(manifest):
        with open(manifest, encoding="utf-8") as f:
            USES.update({k: [tuple(u) for u in v] for k, v in json.load(f).items()})
    for part in parts:
        print(f"{part} ...")
        STEPS[part]()
    with open(manifest, "w", encoding="utf-8", newline="\n") as f:
        json.dump(USES, f, indent=1, sort_keys=True)
    write_credits()
    print(f"done: {len(USES)} files in {OUT}")


if __name__ == "__main__":
    sys.exit(main())
