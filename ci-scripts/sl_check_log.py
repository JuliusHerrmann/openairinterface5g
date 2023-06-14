#!/usr/bin/env python3

import os
import bz2
from typing import Generator

class LogChecker():
    def __init__(self, OPTS, LOGGER):
        self.OPTS = OPTS
        self.LOGGER = LOGGER
        self.rxlog_file_path = os.path.join(OPTS.log_dir, 'rx.log')
        self.txlog_file_path = os.path.join(OPTS.log_dir, 'tx.log')

    def get_lines(self, filename: str) -> Generator[str, None, None]:
        """
        Yield each line of the given log file (.bz2 compressed log file if -c flag is used.)
        """
        fh = bz2.open(filename, 'rb') if self.OPTS.compress else open(filename, 'rb')
        for line_bytes in fh:
            line = line_bytes.decode('utf-8', 'backslashreplace')
            line = line.rstrip('\r\n')
            yield line

    def get_analysis_messages_nearby(self, filename: str) -> Generator[str, None, None]:
        """
        Finding all logs in the log file and yields each line.
        """
        self.LOGGER.info('Scanning %s', filename)
        for line in self.get_lines(filename):
            yield line

    def analyze_nearby_logs(self, exp_nid1: int, exp_nid2: int, sci2: bool) -> bool:
        """
        Checking matched sync logs of Nearby UE.
        """
        found = set()
        est_nid1, est_nid2, time_start_s, time_end_s = -1, -1, -1, -1
        ssb_rsrp = 0
        nb_decoded = 0
        total_rx = 0
        log_file = self.rxlog_file_path
        result = None
        user_msg = None

        if self.OPTS.compress:
            log_file = f'{log_file}.bz2'
        for line in self.get_analysis_messages_nearby(log_file):
            #796821.854505 [NR_PHY] SyncRef UE found with Nid1 10 and Nid2 1 SS-RSRP -100 dBm/RE
            #796811.532881 [NR_PHY] nrUE configured

            if not line.startswith('[') and 'SyncRef UE found' in line and 'Nid1' in line and 'Nid2' in line:
                num_split = 13 if 'RSRP' in line else 10
                fields = line.split(maxsplit=num_split)
                est_nid1 = int(fields[7])
                est_nid2 = int(fields[10])
                if 'RSRP' in line:
                    ssb_rsrp = int(fields[12])
                found.add('found')
                time_end_s = float(fields[0])

            #153092.995494 [NR_PHY]   In nr_ue_sl_pssch_rsrp_measurements: [UE 0] adj_ue_index 0 PSSCH-RSRP: -63 dBm/RE (6685627)
            if not line.startswith('[') and 'PSSCH-RSRP:' in line:
                fields = line.split(maxsplit=11)
                pssch_rsrp = int(fields[-3])

            # 153090.331558 [PHY]   PSSCH test OK with 1 / 2 = 0.50
            if not line.startswith('[') and 'PSSCH test' in line:
                fields = line.split(maxsplit=10)
                nb_decoded = int(fields[-5])
                total_rx = int(fields[-3])

            if time_start_s == -1 and 'nrUE configured' in line:
                fields = line.split(maxsplit=3)
                time_start_s = float(fields[0])

            if sci2:
                if 'the polar decoder output is:' in line:
                    line = line.strip()
                    user_msg = line
                    found.add('found')
            else:
                if 'Received your text! It says:' in line:
                    line = line.strip()
                    user_msg = line
                    found.add('found')

        self.LOGGER.debug('found: %r', found)
        if len(found) != 1:
            self.LOGGER.error(f'Failed -- No SyncRef UE found.')
            return
        if exp_nid1 != est_nid1 or exp_nid2 != est_nid2:
            self.LOGGER.error(f'Failed -- found SyncRef UE Nid1 {est_nid1}, Ni2 {est_nid2}, expecting Nid1 {exp_nid1}, Nid2 {exp_nid2}')
            return
        if time_start_s == -1:
            self.LOGGER.error(f'Failed -- No start time found! Fix log and re-run!')
            return

        delta_time_s = time_end_s - time_start_s
        result = f'SyncRef UE found. PSSCH-RSRP: {pssch_rsrp} dBm/RE. SSS-RSRP: {ssb_rsrp} dBm/RE passed {nb_decoded} total {total_rx} It took {delta_time_s} seconds'
        self.LOGGER.info(result)
        self.LOGGER.info(user_msg)
        return (result, user_msg)

    def get_analysis_messages_syncref(self, filename: str) -> Generator[str, None, None]:
        """
        Finding all logs in the log file with X fields for log parsing optimization
        """
        self.LOGGER.info('Scanning %s', filename)
        for line in self.get_lines(filename):
            #796811.532881 [NR_PHY] nrUE configured
            #796821.854505 [NR_PHY] PSBCH SL generation started
            fields = line.split(maxsplit=5)
            if len(fields) == 4 or len(fields) == 6 :
                yield line

    def analyze_syncref_logs(self, counting_delta: float) -> int:
        """
        Checking logs of SyncRef UE.
        """
        time_start_s, time_end_s = -1, -1
        log_file = self.txlog_file_path
        sum_ssb = 0

        if self.OPTS.compress:
            log_file = f'{log_file}.bz2'
        for line in self.get_analysis_messages_syncref(log_file):
            #796811.532881 [NR_PHY] nrUE configured
            #796821.854505 [NR_PHY] PSBCH SL generation started
            if time_start_s == -1 and 'nrUE configured' in line:
                fields = line.split(maxsplit=2)
                time_start_s = float(fields[0])
                time_end_s = time_start_s + counting_delta
            if 'PSBCH SL generation started' in line:
                fields = line.split(maxsplit=2)
                time_st = float(''.join([ch for ch in fields[0] if ch.isnumeric() or ch =='.']))
                if time_st < time_end_s:
                    sum_ssb += 1
        return sum_ssb
