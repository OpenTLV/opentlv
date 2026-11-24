"""Regression checks for production coverage accounting."""

import unittest

from check_coverage import aggregate_counts


class RepeatedSourceCoverage(unittest.TestCase):
    def test_complementary_hits_do_not_hide_a_missed_outcome_in_either_object(self):
        source = "src/io/internal/diagnostic.h"

        def profile(count, outcomes):
            return {"functions": [], "lines": [
                {"line_number": 14, "count": sum(outcomes),
                 "branches": [{"count": hits} for hits in outcomes]},
                {"line_number": 15, "count": count, "branches": []},
            ]}

        lines, branches, _ = aggregate_counts({source: [
            ("reader.gcno", profile(0, [0, 3])),
            ("writer.gcno", profile(2, [2, 0, 1])),
        ]})

        self.assertEqual(dict(lines), {
            (source, 14, "reader.gcno"): 3, (source, 15, "reader.gcno"): 0,
            (source, 14, "writer.gcno"): 3, (source, 15, "writer.gcno"): 2,
        })
        self.assertEqual(dict(branches), {
            (source, 14, 0, "reader.gcno"): 0,
            (source, 14, 1, "reader.gcno"): 3,
            (source, 14, 0, "writer.gcno"): 2,
            (source, 14, 1, "writer.gcno"): 0,
            (source, 14, 2, "writer.gcno"): 1,
        })
