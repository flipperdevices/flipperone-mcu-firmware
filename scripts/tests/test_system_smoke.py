"""Hosted orchestration tests; no GitHub writes, firmware build or hardware."""
import importlib.util
import json
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

SPEC = importlib.util.spec_from_file_location('system_smoke', Path(__file__).parents[1] / 'system_smoke.py')
smoke = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(smoke)


class Clock:
    def __init__(self):
        self.value = 0
    def now(self):
        return self.value
    def sleep(self, seconds):
        self.value += seconds


class FakeGitHub:
    def __init__(self, conclusion='success', outcome='success'):
        self.calls = []
        self.remote = {'id': 456, 'display_title': 'PR MCU smoke · mcu-123-2',
                       'status': 'completed', 'conclusion': conclusion}
        self.result = {'schema': 1, 'request_id': 'mcu-123-2', 'source_run_id': 123,
            'source_run_attempt': 2, 'firmware_commit': 'a'*40, 'uf2_sha256': 'c'*64,
            'run_id': '456', 'test_outcome': outcome, 'report_url': 'https://reports.example/test'}
        self.missing = False
    def request(self, path, data=None):
        self.calls.append((path, data))
        if '/dispatches' in path or path.endswith('/cancel'):
            return None
        if '/runs?' in path:
            return {'workflow_runs': [
                {'id': 999, 'display_title': 'PR MCU smoke · mcu-another-build'}, self.remote]}
        if '/artifacts?' in path:
            return {'artifacts': [] if self.missing else [{'name': 'mcu-smoke-result-mcu-123-2',
                                                          'expired': False, 'id': 789}]}
        return self.remote
    def receipt(self, artifact_id):
        assert artifact_id == 789
        return self.result


class SystemSmoke(unittest.TestCase):
    def setUp(self):
        self.request = {'schema': 1, 'request_id': 'mcu-123-2', 'run_id': 123, 'run_attempt': 2,
                        'pr_number': 99, 'firmware_commit': 'a'*40, 'uf2_sha256': 'c'*64}
        self.clock = Clock()

    def run_smoke(self, client, **kwargs):
        with patch.object(smoke, 'summary') as summary:
            result = smoke.run(client, self.request, now=self.clock.now, sleep=self.clock.sleep, **kwargs)
            return result, summary

    def test_exact_concurrent_run_and_report_are_linked(self):
        client = FakeGitHub()
        code, summary = self.run_smoke(client)
        self.assertEqual(code, 0)
        self.assertIn('request', client.calls[0][1]['inputs'])
        self.assertTrue(any('actions/runs/456' in call.args[0] for call in summary.call_args_list))
        self.assertTrue(any('Allure report' in call.args[0] for call in summary.call_args_list))
        self.assertFalse(any('/runs/999' in path for path, _ in client.calls))

    def test_remote_failure_or_skipped_tests_cannot_pass(self):
        for conclusion, outcome in [('failure', 'success'), ('cancelled', 'cancelled'),
                                    ('success', 'skipped'), ('success', 'failure')]:
            with self.subTest(conclusion=conclusion, outcome=outcome):
                with self.assertRaisesRegex(ValueError, 'did not pass'):
                    self.run_smoke(FakeGitHub(conclusion, outcome))

    def test_missing_or_wrong_build_receipt_cannot_pass(self):
        client = FakeGitHub()
        client.missing = True
        with self.assertRaisesRegex(ValueError, 'no unique result'):
            self.run_smoke(client)
        client.missing = False
        for field in ('firmware_commit', 'request_id', 'uf2_sha256', 'source_run_attempt', 'run_id'):
            original = client.result[field]
            client.result[field] = 'other build'
            with self.subTest(field=field), self.assertRaisesRegex(ValueError, 'different build'):
                self.run_smoke(client)
            client.result[field] = original

    def test_timeout_cancels_only_the_dispatched_run(self):
        client = FakeGitHub()
        client.remote['status'] = 'queued'
        with self.assertRaisesRegex(ValueError, 'Timed out'):
            self.run_smoke(client, timeout=40)
        self.assertEqual(client.calls[-1], ('/actions/runs/456/cancel', {}))

    def test_interruption_requests_remote_cancellation(self):
        client = FakeGitHub()
        client.remote['status'] = 'in_progress'
        def interrupt(_):
            raise SystemExit(143)
        with patch.object(smoke, 'summary'), self.assertRaises(SystemExit):
            smoke.run(client, self.request, now=self.clock.now, sleep=interrupt)
        self.assertEqual(client.calls[-1], ('/actions/runs/456/cancel', {}))

    def test_request_uses_downloaded_files_and_a_configured_linux_pin(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            event = {'pull_request': {'number': 99,
                'head': {'sha': 'a'*40, 'repo': {'full_name': 'flipperdevices/flipperone-mcu-firmware'}}}}
            (root / 'event.json').write_text(json.dumps(event))
            (root / 'flipper-one-mcu-f2-firmware-pr99.uf2').write_bytes(b'uf2')
            (root / 'flipper-one-mcu-partition-table-pr99.uf2').write_bytes(b'table')
            environment = {'GITHUB_EVENT_PATH': str(root / 'event.json'), 'GITHUB_RUN_ID': '123',
                'GITHUB_RUN_ATTEMPT': '2', 'GITHUB_SHA': 'b'*40,
                'GITHUB_REPOSITORY': 'flipperdevices/flipperone-mcu-firmware', 'FONE_MCU_LINUX_BUILD': '137'}
            with patch.dict(os.environ, environment):
                value = smoke.make_request(root, '456')
                self.assertEqual(value['request_id'], 'mcu-123-2')
                self.assertEqual(value['artifact_id'], 456)
                self.assertEqual(value['linux_build_number'], 137)
                self.assertEqual(value['firmware_commit'], 'a'*40)
                self.assertEqual(value['uf2_sha256'], smoke.hashlib.sha256(b'uf2').hexdigest())
                os.environ['FONE_MCU_LINUX_BUILD'] = ''
                with self.assertRaisesRegex(ValueError, 'FONE_MCU_LINUX_BUILD'):
                    smoke.make_request(root, '456')


if __name__ == '__main__':
    unittest.main()
