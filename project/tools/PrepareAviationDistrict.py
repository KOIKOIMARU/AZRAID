"""Bake AZRAID's aviation districts into shared material batches.

Original geometry. Glazing is separate so it can use reflective surface settings
without making the concrete and painted panels shiny. The flightway reuses the
existing asphalt texture; the buildings and guidance paint are texture-free.
Opening coordinates are relative to the transit bridge at world Z=240;
defense and apron modules use their own fixed district origins.
"""
import json
import math
from pathlib import Path
import struct


MATERIALS = [
    ("ivory_panels", (0.64, 0.66, 0.65, 1), 0.06, 0.72),
    ("blue_black_structure", (0.19, 0.22, 0.25, 1), 0.32, 0.52),
    ("control_glazing", (0.08, 0.13, 0.17, 1), 0.35, 0.22),
    ("oxide_red_identification", (0.36, 0.07, 0.09, 1), 0.08, 0.65),
    ("apron_concrete", (0.30, 0.33, 0.34, 1), 0.02, 0.85),
    ("flightway_markings", (0.78, 0.77, 0.70, 1), 0.0, 0.82),
    ("safety_reflectors", (0.80, 0.53, 0.17, 1), 0.05, 0.70),
    ("flightway_tarmac", (0.86, 0.90, 0.94, 1), 0.0, 0.90),
]


class District:
    def __init__(self):
        self.groups = [{"p": [], "n": [], "i": []} for _ in MATERIALS]

    def face(self, points, material, outward):
        a, b, c = points[:3]
        u = [b[i] - a[i] for i in range(3)]
        v = [c[i] - a[i] for i in range(3)]
        normal = [u[1] * v[2] - u[2] * v[1],
                  u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0]]
        length = math.sqrt(sum(n * n for n in normal))
        assert length > 1e-7
        normal = tuple(n / length for n in normal)
        if sum(normal[i] * outward[i] for i in range(3)) < 0:
            points = list(reversed(points))
            normal = tuple(-n for n in normal)
        g = self.groups[material]
        start = len(g["p"])
        g["p"].extend(points)
        g["n"].extend([normal] * len(points))
        for i in range(1, len(points) - 1):
            g["i"].extend([(start,), (start + i,), (start + i + 1,)])

    def block(self, center, size, material, bevel=0, top_scale=1, lean=(0, 0)):
        x, y, z = center
        w, h, d = (s / 2 for s in size)
        cut = min(bevel, w * 0.45, d * 0.45)
        if cut:
            outline = [(-w + cut, -d), (w - cut, -d), (w, -d + cut),
                       (w, d - cut), (w - cut, d), (-w + cut, d),
                       (-w, d - cut), (-w, -d + cut)]
        else:
            outline = [(-w, -d), (w, -d), (w, d), (-w, d)]
        bottom = [(x + a, y - h, z + b) for a, b in outline]
        top = [(x + a * top_scale + lean[0], y + h,
                z + b * top_scale + lean[1]) for a, b in outline]
        self.face(bottom, material, (0, -1, 0))
        self.face(top, material, (0, 1, 0))
        for i in range(len(outline)):
            j = (i + 1) % len(outline)
            outward = (outline[i][0] + outline[j][0], 0,
                       outline[i][1] + outline[j][1])
            self.face([bottom[i], bottom[j], top[j], top[i]], material, outward)

    def paving(self, width, length, y, z=0, material=7):
        # A single upward face has no doubled vertical end faces at module
        # joins. Adjacent flightway pieces meet at the same top height.
        self.face([(-width / 2, y, z - length / 2),
                   (width / 2, y, z - length / 2),
                   (width / 2, y, z + length / 2),
                   (-width / 2, y, z + length / 2)], material, (0, 1, 0))

    def drum(self, center, radius, height, material, top_radius=None):
        x, y, z = center
        top_radius = radius if top_radius is None else top_radius
        angles = [2 * math.pi * i / 20 for i in range(20)]
        lower = [(x + math.cos(a) * radius, y - height / 2,
                  z + math.sin(a) * radius) for a in angles]
        upper = [(x + math.cos(a) * top_radius, y + height / 2,
                  z + math.sin(a) * top_radius) for a in angles]
        self.face(lower, material, (0, -1, 0))
        self.face(upper, material, (0, 1, 0))
        for i in range(20):
            j = (i + 1) % 20
            self.face([lower[i], lower[j], upper[j], upper[i]], material,
                      (math.cos(angles[i]) + math.cos(angles[j]), 0,
                       math.sin(angles[i]) + math.sin(angles[j])))

    def build(self):
        # A low operations wing and a taller maintenance hangar establish the
        # aviation campus before the bridge. Different setbacks keep the tower
        # visible; all occupied volumes remain beyond the flight envelope.
        x, z = -43, -142
        self.block((x, 0.5, z), (38, 1.0, 60), 4, 2)
        self.block((x, 7.4, z), (34, 13.8, 50), 0, 2.4)
        self.block((x, 1.7, z), (35, 1.8, 51), 1, 2.4)
        self.block((x, 15.3, z), (38, 2.0, 55), 0, 3, top_scale=0.94)
        self.block((x, 13.5, z), (36, 0.65, 52), 1, 2.6)
        # Recessed ribbon windows on the approach and corridor elevations.
        self.block((x, 10.7, z - 25.1), (27, 2.8, 0.22), 2)
        self.block((-25.9, 10.7, z), (0.22, 2.8, 41), 2)
        for offset in (-18, -6, 6, 18):
            self.block((-25.72, 10.7, z + offset), (0.25, 3.0, 0.32), 1)
        for offset in (-10, 0, 10):
            self.block((x + offset, 10.7, z - 25.25), (0.3, 3.0, 0.24), 1)
        for offset in (-20, 20):
            self.block((-25.7, 6.8, z + offset), (0.5, 12.5, 1.2), 0, 0.1)
        self.block((-25.72, 3.6, z + 7), (0.3, 5.2, 6.0), 1)
        self.block((-25.5, 6.5, z + 7), (3.1, 0.7, 9.0), 0, 0.3)
        # Roof equipment has a mounting base and a small screening enclosure.
        self.block((x - 4, 17.0, z + 10), (15, 1.1, 14), 1, 1)
        for offset in (-5, 0, 5):
            self.block((x - 4 + offset, 18.2, z + 10), (3.2, 1.5, 9), 0, 0.35)

        x, z = 50, -96
        self.block((x, 0.45, z), (53, 0.9, 64), 4, 2.5)
        self.block((x, 9.2, z), (48, 17.5, 54), 0, 2.6)
        self.block((x, 1.4, z), (49, 1.6, 55), 1, 2.0)
        self.block((x, 19.3, z), (53, 2.6, 59), 1, 3.0, top_scale=0.92)
        self.block((x, 20.8, z), (51, 0.7, 57), 0, 2.6)
        for bay_z in (z - 13, z + 13):
            self.block((25.84, 7.6, bay_z), (0.3, 12.6, 21), 1)
            self.block((25.62, 7.6, bay_z), (0.16, 11.5, 18.5), 4)
            for y in (3.0, 5.3, 7.6, 9.9, 12.2):
                self.block((25.5, y, bay_z), (0.10, 0.13, 18.5), 1)
            self.block((25.5, 14.7, bay_z), (0.2, 0.8, 19.5), 3)
            self.block((30.0, 0.93, bay_z), (9, 0.04, 0.4), 5)
        for bay_z in (z - 25, z, z + 25):
            self.block((25.8, 9.0, bay_z), (0.7, 17.2, 1.2), 0, 0.12)
        # A setback technical block creates a second depth layer, below the
        # control tower's crown. Its glazing uses the same panel dimensions.
        x, z = -73, -66
        self.block((x, 0.5, z), (27, 1.0, 32), 4, 2)
        self.block((x, 11.0, z), (22, 21.0, 25), 0, 2.2)
        self.block((x, 22.1, z), (25, 1.5, 28), 1, 2.5)
        for y in (8.0, 15.8):
            self.block((x, y, z - 12.62), (17.5, 2.5, 0.22), 2)
            self.block((x + 10.9, y, z), (0.22, 2.5, 19.0), 2)
        self.block((x + 10.9, 11.0, z + 5), (0.5, 20.0, 1.3), 1)

        # Roadside service terraces: solid bases, access cabinets and channel drains.
        # The innermost edge is 18m from the axis, outside the full flight envelope.
        for side in (-1, 1):
            for z, length in ((-196, 44), (-130, 48), (-68, 42)):
                self.block((side * 24, 0.20, z), (12, 0.70, length), 4, 1.2)
                self.block((side * 19.2, 0.85, z), (1.8, 1.0, length - 4), 0, 0.35)
                self.block((side * 19.2, 1.40, z), (1.7, 0.20, length - 4), 1, 0.25)
                for offset in (-length / 2 + 6, length / 2 - 6):
                    self.block((side * 19.1, 1.07, z + offset), (1.9, 0.22, 1.3), 3)
                self.block((side * 27.0, 1.70, z + 10), (3.2, 2.7, 5.0), 1, 0.4)
                self.block((side * 25.35, 1.8, z + 10), (0.08, 1.8, 3.6), 0)

            # Hub foundations and loading halls attach to the existing bridge piers.
            x = side * 43
            self.block((x, 0.55, 1), (28, 1.1, 58), 4, 2)
            self.block((x, 6.1, 3), (24, 11, 44), 0, 2.3)
            self.block((x, 2.0, 3), (25, 1.5, 43), 1, 1.8)
            self.block((x, 12.2, 0), (27, 2.0, 48), 1, 2.3)
            self.block((x, 15.2, 0), (25, 4.0, 45), 2, 2.1)
            self.block((x, 17.8, 0), (28, 1.2, 48), 0, 2.8)
            # Vertical mullions establish glass panes without stretching window textures.
            for offset in (-8, 0, 8):
                self.block((x + offset, 15.3, -22.65), (0.25, 4.1, 0.20), 1)
            for z in (-14, 0, 14):
                self.block((x - side * 12.65, 15.3, z), (0.2, 4.1, 0.25), 1)
                self.block((x - side * 12.1, 5.6, z), (0.20, 6.8, 9), 1)
                self.block((x - side * 12.23, 5.6, z), (0.10, 5.8, 7.7), 2)
            self.block((x, 8.7, -19.9), (15, 2.2, 0.25), 3)
            # Elevated access necks meet the enclosed link at its 20.5m deck.
            self.block((side * 34, 19.6, 0), (17, 2.0, 14), 1, 0.7)
            self.block((side * 34, 22.3, -7), (17, 3.5, 1.1), 0, 0.3)
            self.block((side * 34, 22.3, 7), (17, 3.5, 1.1), 0, 0.3)
            self.block((side * 34, 24.4, 0), (18, 0.9, 16), 1, 0.6)
            for z in (-6.2, 6.2):
                # Bridge access rests on the operations hall, with visible support brackets.
                self.block((side * 34, 17.8, z), (2.0, 3.4, 1.0), 1, 0.18)

        # Asymmetric skyline: one tall, tapered operations tower with an observation crown.
        self.block((-47, 28, 9), (13, 24, 16), 0, 1.8, top_scale=0.72, lean=(0, -2))
        self.block((-47, 28, 0.70), (6.0, 22, 0.35), 1, 0.10, top_scale=0.6)
        self.block((-47, 35.6, 7), (15, 1.2, 18), 3, 1.6)
        self.block((-47, 40.4, 7), (22, 3.6, 24), 1, 3)
        self.block((-47, 43.4, 5.5), (21, 3.5, 23), 2, 3)
        self.block((-47, 46.5, 5.5), (24, 2.1, 25), 0, 3.3, top_scale=0.84)
        for x in (-54, -47, -40):
            self.block((x, 43.5, -6.10), (0.30, 3.5, 0.25), 1)
        self.block((-47, 50.4, 7), (4.0, 5.8, 4.0), 1, 0.5, top_scale=0.6)
        self.block((-47, 56.0, 7), (0.5, 6.5, 0.5), 0)
        self.block((-47, 55.0, 7), (6.0, 0.3, 0.3), 1)

        # Lower communications block balances the tower without mirroring its silhouette.
        self.block((47, 22.6, 12), (14, 9.0, 20), 0, 1.6, top_scale=0.76)
        self.block((47, 27.5, 12), (18, 1.6, 22), 1, 2.0)
        self.block((47, 30.0, 12), (9, 4.0, 10), 3, 1.0, top_scale=0.8)

        # Beyond the underpass, a low servicing apron opens the sky and horizon again.
        for side in (-1, 1):
            x = side * 45
            self.block((x, 0.15, 112), (51, 0.6, 88), 4, 3)
            self.block((side * 61, 5.5, 118), (18, 10.5, 64), 0, 2.0, top_scale=0.84)
            self.block((side * 59.8, 11.0, 118), (23, 1.0, 68), 1, 2.4)
            for z in (96, 118, 140):
                self.block((side * 51.8, 4.8, z), (0.2, 7.2, 15.0), 1)
                self.block((side * 51.6, 4.8, z), (0.2, 6.3, 12.4), 4)
                for y in (2.9, 4.8, 6.7):
                    self.block((side * 51.45, y, z), (0.10, 0.12, 12.4), 1)
                self.block((side * 51.6, 9.0, z), (0.25, 0.7, 13), 3)
                # Service-bay guide marking terminates at the hall, never across the flightway.
                self.block((side * 37, 0.48, z), (22, 0.03, 0.4), 5)
                self.block((side * 26.2, 0.48, z), (0.4, 0.03, 10), 5)

    def build_flightway(self):
        # A flush surface covers the inherited car-lane markings. Guidance
        # belongs to the flightway: two edge lines and one widely spaced axis.
        # The highest point is 11cm above the road base, below every flight path.
        self.paving(24, 450, 0.05, -15)
        for side in (-1, 1):
            self.block((side * 10.8, 0.075, -15), (0.20, 0.02, 450), 5)
        for z in (-206, -158, -110, -62, 34, 82, 130, 178):
            self.block((0, 0.075, z), (0.25, 0.02, 12), 5)
        # The hub's hold-position bars are confined to the service shoulders.
        for side in (-1, 1):
            for z in (-40, 54):
                self.block((side * 8.6, 0.10, z), (2.4, 0.02, 0.35), 5)

    def build_defense(self):
        # A 128m operations block: tall command wing to the left, lower
        # maintenance wing to the right. The enclosed link is supported by both
        # buildings; its underside is over 23m above the flightway in world space.
        x, z = -43, -12
        self.block((x, 0.5, z), (42, 1.0, 86), 4, 2.0)
        self.block((x, 7.4, z - 5), (38, 13.8, 58), 0, 2.3)
        self.block((x, 1.8, z - 5), (39, 2.0, 59), 1, 2.0)
        self.block((x, 14.8, z - 5), (41, 1.3, 61), 1, 2.4)
        self.block((x - 3, 30.0, z + 4), (25, 30, 35), 0, 2.5)
        self.block((x - 3, 46.6, z + 1), (31, 4.0, 40), 1, 3.0)
        self.block((x - 3, 50.6, z - 1), (30, 4.0, 39), 2, 3.0)
        self.block((x - 3, 53.5, z - 1), (34, 1.8, 42), 0, 3.4, top_scale=0.90)
        # The inner vertical spine and roof antenna read at speed as one mass.
        self.block((x + 9.8, 30.5, z + 4), (1.8, 30.5, 4.5), 1, 0.3)
        for y in (22.0, 29.0, 36.0):
            self.block((x - 3, y, z - 13.62), (16.5, 2.7, 0.22), 2)
            self.block((x - 3, y, z + 21.62), (16.5, 2.7, 0.22), 2)
            for offset in (-5.5, 0, 5.5):
                self.block((x - 3 + offset, y, z - 13.82), (0.28, 2.9, 0.26), 1)
                self.block((x - 3 + offset, y, z + 21.82), (0.28, 2.9, 0.26), 1)
        self.block((x - 3, 55.6, z + 3), (5.0, 2.4, 6.0), 1, 0.6)
        self.block((x - 3, 59.5, z + 3), (0.7, 5.5, 0.7), 0)
        self.block((x - 3, 61.3, z + 3), (6.5, 0.3, 0.3), 1)
        self.block((x, 9.9, z - 34.2), (27, 2.9, 0.22), 2)
        self.block((x, 9.9, z + 24.2), (27, 2.9, 0.22), 2)
        self.block((-23.86, 9.9, z - 5), (0.22, 2.9, 46), 2)
        for offset in (-22, -8, 8, 22):
            self.block((-23.68, 9.9, z - 5 + offset), (0.30, 3.1, 0.4), 1)
        for offset in (-10, 0, 10):
            self.block((x - 3 + offset, 50.7, z - 20.65), (0.32, 4.1, 0.26), 1)

        x, z = 43, 11
        self.block((x, 0.5, z), (43, 1.0, 83), 4, 2.0)
        self.block((x, 13.4, z), (40, 25.8, 60), 0, 2.5)
        self.block((x, 1.8, z), (41, 2.0, 61), 1, 2.2)
        self.block((x, 27.2, z), (43, 1.8, 64), 1, 2.7)
        self.block((x + 4, 32.7, z + 9), (24, 9.5, 32), 0, 2.5)
        self.block((x + 4, 38.2, z + 9), (27, 1.8, 35), 1, 2.6)
        for bay_z in (z - 17, z + 15):
            self.block((22.8, 8.4, bay_z), (0.3, 14.2, 23), 1)
            self.block((22.58, 8.4, bay_z), (0.18, 13.1, 20.5), 4)
            for y in (3.0, 5.7, 8.4, 11.1, 13.8):
                self.block((22.45, y, bay_z), (0.12, 0.14, 20.5), 1)
            self.block((22.45, 16.3, bay_z), (0.2, 0.75, 21), 3)
            self.block((29, 1.03, bay_z), (12, 0.04, 0.4), 5)
        self.block((22.82, 22.0, z), (0.22, 3.1, 49), 2)
        self.block((x, 22.0, z - 30.12), (31, 3.1, 0.22), 2)
        self.block((x, 22.0, z + 30.12), (31, 3.1, 0.22), 2)
        for offset in (-12, -4, 4, 12):
            self.block((x + offset, 22.0, z - 30.31), (0.30, 3.3, 0.26), 1)
            self.block((x + offset, 22.0, z + 30.31), (0.30, 3.3, 0.26), 1)
        # A front service portal breaks the broad elevation with a functional
        # recess; repeated horizontal ribs belong to the rolling shutter.
        self.block((x + 2, 8.2, z - 30.22), (24.0, 13.8, 0.25), 1)
        self.block((x + 2, 8.2, z - 30.41), (21.5, 12.5, 0.16), 4)
        for y in (3.0, 5.6, 8.2, 10.8, 13.4):
            self.block((x + 2, y, z - 30.53), (21.5, 0.14, 0.10), 1)
        for offset in (-24, -8, 8, 24):
            self.block((22.62, 22.0, z + offset), (0.30, 3.3, 0.4), 1)
        self.block((x - 4, 7.0, z + 30.22), (15, 11, 0.25), 1)
        self.block((x - 4, 7.0, z + 30.42), (13.5, 10, 0.16), 4)
        for y in (3.0, 5.0, 7.0, 9.0, 11.0):
            self.block((x - 4, y, z + 30.56), (13.5, 0.14, 0.10), 1)

        # Service access pylons and cabinets create a near layer outside +/-17m.
        for side in (-1, 1):
            self.block((side * 20.7, 0.6, -37 + side * 7), (4.0, 1.2, 5.0), 4, 0.7)
            self.block((side * 20.7, 7.2, -37 + side * 7), (2.4, 12.0, 3.3), 0, 0.5)
            self.block((side * 19.44, 7.3, -37 + side * 7), (0.15, 8.5, 1.4), 1)
            self.block((side * 20.7, 13.9, -37 + side * 7), (3.6, 1.4, 4.7), 1, 0.7)
            self.block((side * 20.7, 13.25, -39.46 + side * 7), (2.4, 0.18, 0.05), 6)
            self.block((side * 23, 2.0, 43), (4.8, 3.0, 7.0), 1, 0.7)
            self.block((side * 20.52, 2.1, 43), (0.1, 2.1, 4.5), 0)

        # Elevated connection is a real occupied link, with a floor, glazing,
        # roof and framing. Large openings below it preserve the aiming area.
        self.block((0, 27.1, 8), (64, 1.2, 9), 1, 0.6)
        self.block((0, 29.5, 8), (64, 3.5, 8.6), 2, 0.5)
        self.block((0, 31.8, 8), (65, 1.1, 10), 0, 0.7)
        for x in (-27, -18, -9, 0, 9, 18, 27):
            self.block((x, 29.5, 3.55), (0.35, 3.7, 0.30), 1)
        for side in (-1, 1):
            self.block((side * 30, 28.5, 8), (9, 4.0, 10), 0, 0.6)
            self.block((side * 28, 24.0, 8), (2.0, 5.0, 7), 1, 0.3)

    def build_defense_flightway(self):
        # Raise this surface 10cm over the opening paint in the short overlap;
        # coplanar surfaces must not flicker where the two districts meet.
        self.paving(24, 128, 0.15)
        for side in (-1, 1):
            self.block((side * 10.8, 0.175, 0), (0.20, 0.02, 128), 5)
        for z in (-42, 0, 42):
            self.block((0, 0.175, z), (0.25, 0.02, 12), 5)

    def build_defense_relay(self):
        # Communications court: stepped equipment halls on the left and a
        # slender mast wing on the right. No overhead bridge in this module.
        x, z = -44, -9
        self.block((x, 0.5, z), (44, 1.0, 99), 4, 2.2)
        self.block((x, 10.7, z), (39, 20.4, 86), 0, 2.6)
        self.block((x, 2.0, z), (40, 2.0, 87), 1, 2.2)
        self.block((x, 21.9, z), (43, 2.0, 91), 1, 2.8)
        self.block((x - 6, 29.0, z + 12), (24, 12.2, 36), 0, 2.0)
        self.block((x - 6, 36.5, z + 12), (28, 2.8, 40), 1, 2.6)
        self.block((x - 6, 38.4, z + 12), (26, 1.0, 38), 0, 2.2)
        for y in (9.5, 16.8):
            self.block((-24.37, y, z), (0.22, 2.5, 73), 2)
            for end in (-1, 1):
                self.block((x, y, z + end * 43.12), (29, 2.5, 0.22), 2)
        for bay_z in (-33, -9, 15):
            self.block((-24.18, 13.4, bay_z), (0.45, 17.0, 1.0), 0, 0.1)
        for end in (-1, 1):
            self.block((x - 6, 30.2, z + 12 + end * 18.12), (16, 3.2, 0.22), 2)
        # Screened roof equipment has an actual plinth and intake slats.
        for bay_z in (-34, -15):
            self.block((-45, 23.7, bay_z), (12, 1.6, 10), 1, 0.7)
            self.block((-45, 25.2, bay_z), (10, 1.4, 8), 4, 0.5)
            for offset in (-3, 0, 3):
                self.block((-45 + offset, 26.0, bay_z), (0.7, 0.25, 7), 0)

        x, z = 43, 13
        self.block((x, 0.5, z), (40, 1.0, 81), 4, 2.0)
        self.block((x, 8.3, z), (36, 15.6, 72), 0, 2.4)
        self.block((x, 17.2, z), (40, 2.2, 77), 1, 2.6)
        self.block((x + 3, 29.7, z + 3), (18, 23, 24), 0, 2.0, top_scale=0.88)
        self.block((x + 3, 42.5, z + 3), (24, 2.6, 30), 1, 2.5)
        self.block((x + 3, 44.9, z + 3), (22, 2.2, 28), 2, 2.2)
        self.block((x + 3, 46.6, z + 3), (26, 1.2, 32), 0, 2.7)
        self.block((x + 3, 49.9, z + 3), (1.0, 5.4, 1.0), 1)
        self.block((x + 3, 53.8, z + 3), (0.5, 3.0, 0.5), 0)
        for end in (-1, 1):
            self.block((x, 11.8, z + end * 36.12), (25, 2.5, 0.22), 2)
        self.block((24.88, 11.8, z), (0.22, 2.5, 60), 2)
        self.block((24.82, 4.2, z - 8), (0.25, 5.7, 9), 1)
        self.block((23.4, 7.6, z - 8), (4.0, 0.8, 12), 0, 0.5)
        for y in (24, 32):
            self.block((36.82, y, z + 3), (0.25, 3.0, 15), 2)
        for side in (-1, 1):
            self.block((side * 20, 0.6, -50 + side * 5), (3.0, 1.2, 4.0), 4, 0.5)
            self.block((side * 20, 4.2, -50 + side * 5), (1.2, 6.0, 2.2), 0, 0.25)
            self.block((side * 20, 7.7, -50 + side * 5), (2.2, 1.0, 3.0), 1, 0.35)
            self.block((side * 20, 7.4, -51.55 + side * 5), (1.8, 0.25, 0.07), 6)

    def build_boss_apron(self):
        # The final flight apron has no overhead links. Low servicing halls
        # frame the open sky, with all structures beyond the wide central pad.
        x, z = -71, -25
        self.block((x, 0.65, z), (44, 1.3, 118), 4, 2.5)
        self.block((x, 7.6, z), (40, 13.9, 110), 0, 2.5)
        self.block((x, 2.0, z), (41, 1.5, 111), 1, 2.1)
        self.block((x, 15.5, z), (44, 1.9, 115), 1, 3.0, top_scale=0.92)
        self.block((x, 16.8, z), (42, 0.7, 113), 0, 2.8)
        for bay_z in (-59, -25, 9):
            self.block((-50.85, 7.0, bay_z), (0.30, 11.7, 27), 1)
            self.block((-50.63, 7.0, bay_z), (0.18, 10.6, 24), 4)
            for y in (2.8, 4.9, 7.0, 9.1, 11.2):
                self.block((-50.48, y, bay_z), (0.12, 0.13, 24), 1)
            self.block((-50.55, 13.2, bay_z), (0.2, 0.65, 25), 3)
        self.block((x, 11.8, z - 55.12), (29, 2.1, 0.22), 2)
        self.block((x, 11.8, z + 55.12), (29, 2.1, 0.22), 2)
        for offset in (-10, 0, 10):
            self.block((x + offset, 11.8, z - 55.30), (0.28, 2.3, 0.25), 1)
            self.block((x + offset, 11.8, z + 55.30), (0.28, 2.3, 0.25), 1)
        for offset in (-26, 26):
            self.block((x - 5, 17.9, z + offset), (12, 1.5, 15), 1, 1.0)
            self.block((x - 5, 19.1, z + offset), (10, 1.0, 12), 0, 0.7)

        # A shorter dispatch wing sits farther along the opposite edge, so
        # the two sides do not form another symmetrical canyon.
        x, z = 72, 48
        self.block((x, 0.65, z), (43, 1.3, 74), 4, 2.5)
        self.block((x, 7.4, z), (38, 13.5, 65), 0, 2.4)
        self.block((x, 2.0, z), (39, 1.5, 66), 1, 2.1)
        self.block((x, 14.8, z), (42, 1.5, 70), 1, 2.8)
        self.block((x + 3, 17.1, z + 6), (23, 3.0, 36), 2, 2.0)
        self.block((x + 3, 19.4, z + 6), (27, 1.5, 40), 0, 2.7)
        self.block((52.88, 10.3, z), (0.22, 2.6, 53), 2)
        for offset in (-22, -7, 7, 22):
            self.block((52.70, 10.3, z + offset), (0.26, 2.8, 0.32), 1)
        for offset in (-8, 0, 8):
            self.block((x + 3 + offset, 17.1, z - 12.18), (0.28, 3.2, 0.26), 1)
        self.block((x, 10.3, z - 32.62), (28, 2.6, 0.22), 2)
        self.block((x, 10.3, z + 32.62), (28, 2.6, 0.22), 2)
        self.block((52.82, 4.0, z + 3), (0.25, 5.4, 8.0), 1)
        self.block((51.0, 7.3, z + 3), (5.0, 0.8, 11), 0, 0.6)

        # Sparse functional near objects keep a motion cue on the open pad.
        # Nothing occupies the aiming corridor or rises behind the boss.
        for side in (-1, 1):
            for z in (-98 + side * 12, 102 + side * 12):
                self.block((side * 38, 0.6, z), (2.8, 1.2, 3.3), 1, 0.4)
                self.block((side * 38, 3.8, z), (0.55, 5.2, 0.65), 0, 0.10)
                self.block((side * 38, 6.8, z), (2.7, 0.8, 0.85), 1, 0.20)
                self.block((side * 38, 6.8, z - 0.45), (2.1, 0.30, 0.07), 6)
            self.block((side * 43, 1.3, 63 - side * 35), (4.8, 2.6, 6.8), 1, 0.7)
            self.block((side * 40.53, 1.4, 63 - side * 35), (0.10, 1.9, 4.7), 0)
            self.block((side * 87, 0.65, 0), (1.2, 1.3, 256), 4)
            self.block((side * 87, 1.37, 0), (1.25, 0.15, 256), 0)

    def build_boss_apron_freight(self):
        # A recessed cargo dock and screened servicing plant replace the long
        # hangar/dispatch pair. Shared materials, different masses and roofline.
        x, z = -68, -12
        self.block((x, 0.8, z), (48, 1.6, 114), 4, 2.5)
        self.block((x - 3, 8.8, z), (36, 16, 96), 0, 2.8)
        self.block((x - 3, 2.8, z), (37, 2, 97), 1, 2.2)
        self.block((x - 3, 18.2, z), (41, 2.8, 102), 1, 3.0, top_scale=0.84)
        self.block((x - 3, 20.0, z), (38, 0.8, 99), 0, 2.7)
        # The supported canopy shelters three raised loading bays.
        self.block((-49, 12.7, z), (15, 1.2, 97), 1, 1.0)
        self.block((-49, 13.5, z), (15.5, 0.5, 98), 0, 1.0)
        for bay_z in (-44, -12, 20):
            self.block((-52.88, 7.0, bay_z), (0.24, 10.7, 22), 1)
            self.block((-52.67, 7.0, bay_z), (0.16, 9.6, 19), 4)
            for y in (3.2, 5.1, 7, 8.9, 10.8):
                self.block((-52.54, y, bay_z), (0.10, 0.13, 19), 1)
            self.block((-47.5, 1.1, bay_z), (11, 0.6, 24), 1, 0.7)
        for bay_z in (-59, -28, 4, 35):
            self.block((-43, 6.7, bay_z), (0.8, 12.6, 0.8), 0, 0.1)
        for end in (-1, 1):
            self.block((x - 3, 13.8, z + end * 48.12), (26, 2.7, 0.22), 2)
        for bay_z in (48, 68):
            self.block((-60, 0.6, bay_z), (14, 1.2, 16), 4, 1.0)
            self.block((-60, 3.0, bay_z), (11, 3.6, 13), 1, 0.6)
            self.block((-60, 5.0, bay_z), (11.5, 0.4, 13.5), 0, 0.6)
            self.block((-54.40, 3.0, bay_z), (0.18, 2.6, 9), 3)

        x, z = 71, 44
        self.block((x, 0.65, z), (40, 1.3, 60), 4, 2.2)
        self.block((x, 6.3, z), (34, 11.3, 50), 0, 2.5)
        self.block((x, 12.8, z), (38, 1.7, 55), 1, 2.8)
        self.block((x - 2, 14.8, z + 4), (25, 2.3, 33), 4, 1.2)
        for offset in (-8, 0, 8):
            self.block((x - 2 + offset, 16.2, z + 4), (0.8, 0.5, 29), 0)
        self.block((53.88, 8.3, z), (0.22, 2.1, 38), 2)
        for end in (-1, 1):
            self.block((x, 8.3, z + end * 25.12), (24, 2.1, 0.22), 2)
        self.block((53.82, 3.5, z - 6), (0.25, 4.5, 7), 1)
        # Low reservoirs on plinths, with a restrained identification band.
        for tank_z in (-64, -40):
            self.block((76, 0.6, tank_z), (18, 1.2, 18), 4, 2.0)
            self.drum((76, 6.6, tank_z), 7.0, 10.8, 0)
            self.drum((76, 7.7, tank_z), 7.08, 0.65, 3)
            self.drum((76, 12.6, tank_z), 7.2, 1.2, 1, 4.8)
            self.block((76, 13.6, tank_z), (2.0, 0.8, 2.0), 0, 0.3)
        for side in (-1, 1):
            for z in (-102 + side * 12, 99 + side * 12):
                self.block((side * 38, 0.6, z), (2.8, 1.2, 3.3), 1, 0.4)
                self.block((side * 38, 3.8, z), (0.55, 5.2, 0.65), 0, 0.1)
                self.block((side * 38, 6.8, z), (2.7, 0.8, 0.85), 1, 0.2)
                self.block((side * 38, 6.8, z - 0.45), (2.1, 0.3, 0.07), 6)
            self.block((side * 87, 0.65, 0), (1.2, 1.3, 256), 4)
            self.block((side * 87, 1.37, 0), (1.25, 0.15, 256), 0)

    def build_boss_apron_surface(self):
        # A broad apron covers the car road after its guardrails terminate.
        # Sparse bay markings leave the central combat ground visually quiet.
        self.paving(174, 256, 0.265)
        for side in (-1, 1):
            self.block((side * 65, 0.295, 0), (43, 0.02, 256), 4)
            self.block((side * 43.4, 0.335, 0), (0.22, 0.02, 256), 5)
            for z in (-77, -25, 27, 79):
                self.block((side * 53, 0.335, z), (19, 0.02, 0.30), 5)
                self.block((side * 43.7, 0.335, z), (0.30, 0.02, 11), 5)
        for z in (-74, 54):
            self.block((0, 0.335, z), (0.25, 0.02, 12), 5)

    def save(self, output, asset_name, selected, surface=False, overhead=False, surface_ceiling=0.12,
             surface_period=0):
        output.mkdir(parents=True, exist_ok=True)
        selected = {index for index in selected if self.groups[index]["p"]}
        doc = {"asset": {"version": "2.0", "generator": "AZRAID PrepareAviationDistrict.py"},
               "scene": 0, "scenes": [{"nodes": [0]}], "nodes": [{"name": asset_name, "mesh": 0}],
               "meshes": [{"primitives": []}], "materials": [], "accessors": [], "bufferViews": []}
        binary = bytearray()

        def accessor(values, kind, indices=False):
            while len(binary) % 4:
                binary.append(0)
            offset = len(binary)
            fmt = "<" + ("I" if indices else "f") * len(values[0])
            for value in values:
                binary.extend(struct.pack(fmt, *value))
            view = len(doc["bufferViews"])
            doc["bufferViews"].append({"buffer": 0, "byteOffset": offset, "byteLength": len(binary) - offset})
            index = len(doc["accessors"])
            doc["accessors"].append({"bufferView": view, "componentType": 5125 if indices else 5126,
                "count": len(values), "type": kind,
                "min": [min(v[i] for v in values) for i in range(len(values[0]))],
                "max": [max(v[i] for v in values) for i in range(len(values[0]))]})
            return index

        for index, (material_name, color, metallic, roughness) in enumerate(MATERIALS):
            if index not in selected:
                continue
            g = self.groups[index]
            material = len(doc["materials"])
            material_doc = {"name": material_name, "pbrMetallicRoughness": {
                "baseColorFactor": color, "metallicFactor": metallic, "roughnessFactor": roughness}}
            attributes = {"POSITION": accessor(g["p"], "VEC3"), "NORMAL": accessor(g["n"], "VEC3")}
            if material_name == "flightway_tarmac":
                # Six metres per tile: the inherited grain gives motion cues
                # without large stains or repeating car-lane graphics.
                texture_uri = ("../../free_models/Downtown City MegaKit[Standard]/"
                               "Exports/glTF (Godot)/T_Concrete_Asphalt_BaseColor.png")
                assert (output / texture_uri).is_file()
                doc["images"] = [{"uri": texture_uri}]
                doc["samplers"] = [{"wrapS": 10497, "wrapT": 10497}]
                doc["textures"] = [{"source": 0, "sampler": 0}]
                material_doc["pbrMetallicRoughness"]["baseColorTexture"] = {"index": 0}
                # Integer tile counts make the grain meet across repeating
                # 128m/256m modules instead of restarting partway through a tile.
                period_z = surface_period / round(surface_period / 6) if surface_period else 6
                attributes["TEXCOORD_0"] = accessor([(p[0] / 6, p[2] / period_z) for p in g["p"]], "VEC2")
            doc["materials"].append(material_doc)
            doc["meshes"][0]["primitives"].append({"material": material,
                "attributes": attributes, "indices": accessor(g["i"], "SCALAR", True)})
            # Verify faces, not just their vertices, stay outside the playable corridor.
            for start in range(0, len(g["i"]), 3):
                triangle = [g["p"][i[0]] for i in g["i"][start:start + 3]]
                if surface:
                    assert all(0.0 <= v[1] <= surface_ceiling for v in triangle)
                else:
                    outside = min(v[0] for v in triangle) > 17 or max(v[0] for v in triangle) < -17
                    assert outside or (overhead and min(v[1] for v in triangle) >= 26.5)
        vertices = [v for index, g in enumerate(self.groups) if index in selected for v in g["p"]]
        bounds = [[min(v[i] for v in vertices) for i in range(3)],
                  [max(v[i] for v in vertices) for i in range(3)]]
        assert -250 < bounds[0][2] and bounds[1][2] < 250
        doc["buffers"] = [{"uri": asset_name + ".bin", "byteLength": len(binary)}]
        (output / (asset_name + ".bin")).write_bytes(binary)
        (output / (asset_name + ".gltf")).write_text(json.dumps(doc, indent=2) + "\n", encoding="utf-8")
        print(f"{asset_name}: {len(vertices)} vertices, {len(selected)} batches, {len(binary)} bytes")
        print("Bounds:", bounds, "flush flightway" if surface else "flight corridor clear")


if __name__ == "__main__":
    district = District()
    district.build()
    output = Path(__file__).resolve().parents[1] / "resources/scenery/aviation_district"
    district.save(output, "AviationDistrict", {0, 1, 3, 4, 5, 6})
    district.save(output, "AviationGlazing", {2})
    flightway = District()
    flightway.build_flightway()
    flightway.save(output, "AviationFlightway", {5, 7}, surface=True)
    defense = District()
    defense.build_defense()
    defense.save(output, "AviationDefense", {0, 1, 3, 4, 5, 6}, overhead=True)
    defense.save(output, "AviationDefenseGlazing", {2}, overhead=True)
    defense_flightway = District()
    defense_flightway.build_defense_flightway()
    defense_flightway.save(output, "AviationDefenseFlightway", {5, 7}, surface=True, surface_ceiling=0.22,
                          surface_period=128)
    relay = District()
    relay.build_defense_relay()
    relay.save(output, "AviationDefenseRelay", {0, 1, 3, 4, 5, 6})
    relay.save(output, "AviationDefenseRelayGlazing", {2})
    apron = District()
    apron.build_boss_apron()
    apron.save(output, "AviationBossApron", {0, 1, 3, 4, 5, 6})
    apron.save(output, "AviationBossApronGlazing", {2})
    freight = District()
    freight.build_boss_apron_freight()
    freight.save(output, "AviationBossApronFreight", {0, 1, 3, 4, 5, 6})
    freight.save(output, "AviationBossApronFreightGlazing", {2})
    apron_surface = District()
    apron_surface.build_boss_apron_surface()
    apron_surface.save(output, "AviationBossApronSurface", {4, 5, 7}, surface=True, surface_ceiling=0.36,
                       surface_period=256)
    terrain = District()
    terrain.paving(1600, 256, 0, material=4)
    terrain.save(output, "AviationTerrain", {4}, surface=True, surface_ceiling=0)
