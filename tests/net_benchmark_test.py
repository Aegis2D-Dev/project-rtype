"""Tests of the report of tools/net_benchmark.py: python3 -m unittest net_benchmark_test."""

import argparse
import csv
import sys
import tempfile
import unittest
from unittest import mock
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "tools"))
import net_benchmark  # noqa: E402


def line(label: str, profile: str, bytes_in: str, rtt: str) -> dict[str, str]:
    values = dict.fromkeys(net_benchmark.FIELDS, "")
    values.update(machine="Test, 8 cores", os="Linux", label=label, profile=profile,
                  delay_ms="150", loss_set_percent="2", bots="4", seconds="30.0",
                  bytes_in_per_s=bytes_in, bytes_out_per_s="1140.00", rtt_ms=rtt,
                  rtt_jitter_ms="2.00", loss_percent="1.50", snapshot_jitter_ms="4.00")
    return values


class ReportTest(unittest.TestCase):
    def setUp(self) -> None:
        self.folder = tempfile.TemporaryDirectory()
        root = Path(self.folder.name)
        self.args = argparse.Namespace(results=root / "results", page=root / "page.md",
                                       images=root / "images")
        self.args.results.mkdir()
        self.args.images.mkdir()
        self.args.page.write_text("# Page\n\n<!-- results:begin -->\nold\n<!-- results:end -->\n"
                                  "after\n")

    def tearDown(self) -> None:
        self.folder.cleanup()

    def write(self, lines: list[dict[str, str]]) -> None:
        with (self.args.results / "test-8-cores.csv").open("w", newline="") as file:
            writer = csv.DictWriter(file, fieldnames=net_benchmark.FIELDS, lineterminator="\n")
            writer.writeheader()
            writer.writerows(lines)

    def test_a_label_is_compared_with_the_first(self) -> None:
        self.write([line("baseline", "lan", "10000.00", "10.00"),
                    line("baseline", "150ms-2%", "10000.00", "310.00"),
                    line("delta-snapshots", "150ms-2%", "6000.00", "310.00")])
        net_benchmark.report(self.args)
        page = self.args.page.read_text()
        self.assertIn("### Test, 8 cores", page)
        self.assertIn("| lan | baseline | 10000.00 | 1140.00 | 10.00 |", page)
        self.assertIn("| 150ms-2% | delta-snapshots | 6000.00 (-40 %) | 1140.00 | 310.00 |",
                      page)
        self.assertNotIn("old", page)
        self.assertTrue(page.endswith("<!-- results:end -->\nafter\n"))
        for figure in ("bandwidth", "rtt"):
            chart = (self.args.images / f"network-benchmark-test-8-cores-{figure}.svg").read_text()
            self.assertIn("delta-snapshots", chart)
            self.assertIn("150ms-2%", chart)
            self.assertIn(f"images/network-benchmark-test-8-cores-{figure}.svg", page)
        # Run again: the same page.
        net_benchmark.report(self.args)
        self.assertEqual(page, self.args.page.read_text())

    def test_a_run_replaces_only_its_own_lines(self) -> None:
        old = [line("baseline", "lan", "1", "1"), line("baseline", "50ms", "2", "2"),
               line("prediction", "lan", "3", "3")]
        merged = net_benchmark.merge(old, [line("baseline", "lan", "9", "9")])
        self.assertEqual([(entry["label"], entry["profile"], entry["rtt_ms"]) for entry in merged],
                         [("baseline", "50ms", "2"), ("prediction", "lan", "3"),
                          ("baseline", "lan", "9")])

    def test_a_run_keeps_the_other_labels_of_its_csv(self) -> None:
        target = self.args.results / "test-8-cores.csv"
        self.write([line("baseline", "lan", "10000.00", "10.00")])
        run = argparse.Namespace(profiles="lan", netem=None, build=Path("build"), os="Linux",
                                 name="Test, 8 cores", label="lz4", bots=4, seconds=30,
                                 dry_run=False, output=target)
        measured = line("lz4", "lan", "9000.00", "10.00")
        fields = {key: value for key, value in measured.items()
                  if key not in ("machine", "os", "label")}
        with mock.patch.object(net_benchmark, "measure", return_value=fields), \
                mock.patch.object(net_benchmark.time, "sleep"):
            net_benchmark.run(run)
        with target.open(newline="") as file:
            labels = [(entry["label"], entry["bytes_in_per_s"]) for entry in csv.DictReader(file)]
        self.assertEqual(labels, [("baseline", "10000.00"), ("lz4", "9000.00")])

    def test_changes_are_shown_only_when_they_mean_something(self) -> None:
        self.assertEqual(net_benchmark.change("120.00", "100.00"), "120.00 (+20 %)")
        self.assertEqual(net_benchmark.change("100.00", "100.00"), "100.00")
        self.assertEqual(net_benchmark.change("5.00", "0.00"), "5.00")
        self.assertEqual(net_benchmark.change("", "1.00"), "-")

    def test_no_results_stops_the_report(self) -> None:
        with self.assertRaises(SystemExit):
            net_benchmark.report(self.args)


if __name__ == "__main__":
    unittest.main()
