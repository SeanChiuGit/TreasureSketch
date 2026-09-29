"""Asset-free Canyon navigation sketches. Run: python Scripts/prototype_canyon_plan.py.

This is a design probe, not the UE runtime generator. The graph and clue come first;
world coordinates, terrain and meshes are separate outputs.
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass, field
from html import escape
from math import atan2, cos, hypot, pi, sin
from pathlib import Path
from random import Random
from textwrap import wrap


@dataclass
class Place:
    name: str
    role: str
    x: float
    y: float
    z: float = 0.0
    landmark: str = ""


@dataclass
class Passage:
    a: str
    b: str
    character: str


@dataclass
class Plan:
    seed: int
    problem: str
    places: dict[str, Place] = field(default_factory=dict)
    passages: list[Passage] = field(default_factory=list)
    clue: str = ""

    def place(self, name: str, role: str, x: float, y: float, z: float = 0.0,
              landmark: str = "") -> None:
        self.places[name] = Place(name, role, x, y, z, landmark)

    def link(self, a: str, b: str, character: str = "floor") -> None:
        self.passages.append(Passage(a, b, character))


def generate(seed: int) -> Plan:
    rng = Random(seed)
    # The choice is random. A seed does not select a fixed map by modulo.
    problem = rng.choices(
        ["Fork and rejoin", "Loop and shortcut", "High and low", "Hub and pocket",
         "Chain and alcoves", "Braided gorge"],
        weights=[2, 2, 2, 2, 2, 2],
    )[0]
    plan = Plan(seed, problem)
    length = rng.uniform(125, 190)
    branch = rng.uniform(32, 65) * rng.choice([-1, 1])
    fork_x = length * rng.uniform(0.24, 0.34)
    merge_x = length * rng.uniform(0.70, 0.82)
    early = rng.choice(["Needle", "Split Peak", "Twin Pillars"])
    crossing = rng.choice(["Arch", "Broken Bridge"])
    plan.place("spawn", "spawn", 0, 0)
    plan.place("first", "landmark", fork_x * 0.52, rng.uniform(-10, 10), landmark=early)
    plan.place("fork", "decision", fork_x, 0)
    plan.link("spawn", "first")
    plan.link("first", "fork")

    if problem == "Fork and rejoin":
        plan.place("high", "route", (fork_x + merge_x) * 0.52, branch,
                   rng.uniform(3, 6), crossing)
        plan.place("low", "route", (fork_x + merge_x) * 0.57, -branch * rng.uniform(0.65, 1.05),
                   -rng.uniform(2, 4), "Stone Ring")
        plan.place("merge", "decision", merge_x, rng.uniform(-8, 8))
        plan.link("fork", "high", "high")
        plan.link("high", "merge", "high")
        plan.link("fork", "low", "low")
        plan.link("low", "merge", "low")
        chosen = rng.choice(["high", "low"])
        plan.clue = f"From {early}, choose the {chosen} route at the fork; pass {plan.places[chosen].landmark}, then search beyond the reunion."
    elif problem == "Loop and shortcut":
        plan.place("arc1", "route", fork_x + (merge_x - fork_x) * 0.28, branch * 0.75,
                   landmark=crossing)
        plan.place("arc2", "viewpoint", fork_x + (merge_x - fork_x) * 0.72, branch,
                   landmark="Twin Pillars" if early != "Twin Pillars" else "Needle")
        plan.place("merge", "decision", merge_x, rng.uniform(-10, 10))
        plan.link("fork", "arc1", "long")
        plan.link("arc1", "arc2", "long")
        plan.link("arc2", "merge", "long")
        plan.link("fork", "merge", "shortcut")
        plan.place("treasure", "treasure", merge_x + rng.uniform(12, 27), branch * 1.38)
        plan.link("arc2", "treasure", "hidden pocket")
        plan.clue = f"From {early}, take the outer loop past {crossing}; the shortcut skips the treasure pocket near the second landmark."
    elif problem == "High and low":
        plan.place("ridge", "viewpoint", fork_x + (merge_x - fork_x) * 0.44, branch,
                   rng.uniform(4, 6), "Split Peak" if early != "Split Peak" else "Needle")
        plan.place("wash", "route", fork_x + (merge_x - fork_x) * 0.53, -branch * 0.8,
                   -rng.uniform(2, 4), crossing)
        plan.place("lookout", "viewpoint", merge_x, branch * rng.uniform(0.8, 1.2),
                   rng.uniform(4, 6))
        plan.place("bend", "route", merge_x, -branch * rng.uniform(0.55, 0.95),
                   -rng.uniform(2, 4))
        plan.place("treasure", "treasure", length, -branch * rng.uniform(0.45, 0.8),
                   -rng.uniform(2, 4))
        plan.link("fork", "ridge", "ridge")
        plan.link("ridge", "lookout", "ridge")
        plan.link("fork", "wash", "wash")
        plan.link("wash", "bend", "wash")
        plan.link("bend", "treasure", "wash")
        plan.clue = f"The ridge beyond {early} reveals the treasure below; return to the fork and follow the wash under {crossing}."
    elif problem == "Hub and pocket":
        plan.place("north", "viewpoint", fork_x + rng.uniform(8, 23), branch,
                   landmark=crossing)
        plan.place("south", "dead end", fork_x + rng.uniform(6, 26), -branch * 0.9,
                   landmark="Stone Ring")
        plan.place("pocket", "treasure area", merge_x, branch * rng.uniform(0.2, 0.6))
        plan.place("merge", "decision", merge_x * 0.78, branch * 0.13)
        plan.link("fork", "north", "rim")
        plan.link("north", "merge", "rim")
        plan.link("fork", "south", "dead end")
        plan.link("fork", "merge", "floor")
        plan.link("merge", "pocket", "hidden pocket")
        plan.clue = f"From {early}, use {crossing} to find the hub; ignore the Stone Ring dead end and search the side pocket."
    elif problem == "Chain and alcoves":
        plan.place("hall1", "route", fork_x + (merge_x - fork_x) * 0.28,
                   rng.uniform(-16, 16), landmark=crossing)
        plan.place("hall2", "route", fork_x + (merge_x - fork_x) * 0.75,
                   rng.uniform(-16, 16), landmark="Stone Ring")
        plan.place("alcove1", "dead end", plan.places["hall1"].x + rng.uniform(-8, 12), branch)
        plan.place("alcove2", "dead end", plan.places["hall2"].x + rng.uniform(-8, 12), -branch)
        plan.place("end", "dead end", length, rng.uniform(-8, 8))
        plan.link("fork", "hall1")
        plan.link("hall1", "hall2")
        plan.link("hall2", "end")
        plan.link("hall1", "alcove1", "hidden pocket")
        plan.link("hall2", "alcove2", "hidden pocket")
        chosen = rng.choice(["alcove1", "alcove2"])
        plan.places[chosen].role = "treasure"
        plan.clue = f"Follow the canyon from {early}; search the side alcove by {crossing if chosen == 'alcove1' else 'Stone Ring'}, before the dead end."
    else:  # Braided gorge: three parallel routes with cross-connections.
        plan.place("upper1", "route", fork_x + (merge_x - fork_x) * 0.32, branch,
                   landmark=crossing)
        plan.place("upper2", "route", fork_x + (merge_x - fork_x) * 0.76, branch * 0.87)
        plan.place("middle", "decision", fork_x + (merge_x - fork_x) * 0.5,
                   rng.uniform(-8, 8), landmark="Stone Ring")
        plan.place("lower1", "route", fork_x + (merge_x - fork_x) * 0.25, -branch)
        plan.place("lower2", "route", fork_x + (merge_x - fork_x) * 0.72, -branch * 0.95,
                   landmark="Needle" if early != "Needle" else "Split Peak")
        plan.place("far", "decision", merge_x, rng.uniform(-8, 8))
        plan.place("treasure", "treasure", length, branch * rng.choice([-0.7, 0.7]))
        for a, b, kind in [
            ("fork", "upper1", "rim"), ("upper1", "upper2", "rim"),
            ("upper2", "far", "rim"), ("fork", "middle", "floor"),
            ("middle", "far", "floor"), ("fork", "lower1", "wash"),
            ("lower1", "lower2", "wash"), ("lower2", "far", "wash"),
            ("upper1", "middle", "crossing"), ("middle", "lower2", "crossing"),
            ("far", "treasure", "goal")]:
            plan.link(a, b, kind)
        plan.clue = f"From {early}, weave between three routes: cross at {crossing}, then use the Stone Ring to reach the far end."

    if problem == "Fork and rejoin":
        plan.place("treasure", "treasure", length,
                   plan.places["merge"].y + rng.uniform(-14, 14))
        plan.link("merge", "treasure", "goal")
    if problem == "Hub and pocket":
        plan.places["pocket"].role = "treasure"

    # Continuous embedding: variable distances, curved branch positions, and
    # whole-map rotation. No tile positions or asset socket sizes enter the plan.
    angle = rng.uniform(-pi, pi)
    scale = rng.uniform(0.85, 1.18)
    for place in plan.places.values():
        x, y = place.x * scale, place.y * scale
        place.x = x * cos(angle) - y * sin(angle)
        place.y = x * sin(angle) + y * cos(angle)
    validate(plan)
    return plan


def validate(plan: Plan) -> None:
    assert plan.places["spawn"].role == "spawn"
    goals = [p for p in plan.places.values() if p.role == "treasure"]
    assert len(goals) == 1
    seen = {"spawn"}
    frontier = ["spawn"]
    while frontier:
        current = frontier.pop()
        for edge in plan.passages:
            other = edge.b if edge.a == current else edge.a if edge.b == current else None
            if other and other not in seen:
                seen.add(other)
                frontier.append(other)
    assert len(seen) == len(plan.places)
    assert goals[0].name in seen
    assert len({p.landmark for p in plan.places.values() if p.landmark}) >= 3
    assert plan.places["first"].landmark
    assert any(p.role == "decision" for p in plan.places.values())
    for edge in plan.passages:
        a, b = plan.places[edge.a], plan.places[edge.b]
        distance = hypot(a.x - b.x, a.y - b.y)
        assert distance > 8 and abs(a.z - b.z) / distance < 0.20


def svg_card(plan: Plan, left: int, top: int, width: int = 400, height: int = 280) -> str:
    xs = [p.x for p in plan.places.values()]
    ys = [p.y for p in plan.places.values()]
    extent = max(max(xs) - min(xs), max(ys) - min(ys), 1)
    scale = min((width - 85) / extent, (height - 125) / extent)
    mid_x, mid_y = (min(xs) + max(xs)) / 2, (min(ys) + max(ys)) / 2

    def xy(place: Place) -> tuple[float, float]:
        return (left + width / 2 + (place.x - mid_x) * scale,
                top + (height - 15) / 2 + (place.y - mid_y) * scale)

    out = [f'<g><rect x="{left}" y="{top}" width="{width}" height="{height}" rx="8" fill="#f7f0df" stroke="#a69b87"/>',
           f'<text x="{left+14}" y="{top+22}" class="title">Seed {plan.seed} · {escape(plan.problem)}</text>']
    colors = {"high": "#b45435", "ridge": "#b45435", "long": "#b45435",
              "low": "#377b8c", "wash": "#377b8c", "shortcut": "#377b8c",
              "dead end": "#a8a29a", "hidden pocket": "#714778"}
    for edge in plan.passages:
        x1, y1 = xy(plan.places[edge.a])
        x2, y2 = xy(plan.places[edge.b])
        out.append(f'<line x1="{x1:.1f}" y1="{y1:.1f}" x2="{x2:.1f}" y2="{y2:.1f}" stroke="{colors.get(edge.character, "#655f53")}" stroke-width="5" stroke-linecap="round"/>')
    for place in plan.places.values():
        x, y = xy(place)
        color = "#e9bc4d" if place.role == "spawn" else "#d95647" if place.role == "treasure" else "#fffaf0"
        out.append(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="7" fill="{color}" stroke="#393832" stroke-width="2"/>')
        if place.landmark or place.role in {"spawn", "treasure"}:
            label = place.landmark or place.role
            out.append(f'<text x="{x+9:.1f}" y="{y-9:.1f}" class="label">{escape(label)}</text>')
    clue_lines = wrap(plan.clue, width=63, break_long_words=False)
    for index, line in enumerate(clue_lines[:2]):
        out.append(f'<text x="{left+14}" y="{top+height-32+15*index}" class="clue">{escape(line)}</text>')
    out.append('</g>')
    return "".join(out)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--start", type=int, default=1000)
    parser.add_argument("--count", type=int, default=20)
    parser.add_argument("--out", type=Path, default=Path("Docs/CanyonGraybox20.svg"))
    args = parser.parse_args()
    plans = [generate(seed) for seed in range(args.start, args.start + args.count)]
    columns = 4
    rows = (len(plans) + columns - 1) // columns
    cards = "".join(svg_card(plan, (index % columns) * 410 + 10,
                             (index // columns) * 290 + 10)
                    for index, plan in enumerate(plans))
    svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{columns*410+10}" height="{rows*290+10}" '
           f'viewBox="0 0 {columns*410+10} {rows*290+10}">'
           '<style>.title{font:600 15px sans-serif;fill:#272a28}.label{font:12px sans-serif;fill:#2e312d}'
           '.clue{font:11px sans-serif;fill:#4f4b47}</style>' + cards + '</svg>')
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(svg, encoding="utf-8")
    counts = {name: sum(p.problem == name for p in plans) for name in sorted({p.problem for p in plans})}
    print(f"Generated {len(plans)} connected plans: {counts}")
    print(args.out.resolve())


if __name__ == "__main__":
    main()
