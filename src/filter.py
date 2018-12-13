from collections import namedtuple, OrderedDict
import os
import shutil
import subprocess

from cluster import Cluster
from config import Directory
from utils import move_replace
from pssm_class import PSSM_meme, PSSM_conv
import pickle
import re

FilterIntDir = namedtuple(
    'FilterIntDir', "post_evalue post_entropy post_corr mast_shortseq "
                    "mast_postcorr cluster_pkl")

class Filter:
    def __init__(self, dir):
        self.dir = dir
        self._dir = self.set_internal_dir()
        self.cluster = None
        self.switches = self.set_switches()

    def set_switches(self):
        switches = OrderedDict()
        # Get input_seqs
        switches['EVALUE'] = (False, self.screen_evalue)
        switches['ENTROPY'] = (False, self.screen_entropy)
        switches['CORRELATED'] = (False, self.screen_correlated)
        switches['NON_COMBI'] = (False, self.screen_non_combi)
        return switches

    def run(self):
        for (to_run, func) in self.switches.values():
            if to_run:
                try:
                    print(f"Filter/{func.__name__}:")
                    func()
                    print("Filter/Success!\n")
                except:
                    print("Filter/Error: {}".format(func.__name__))
                    raise
        return

    def set_internal_dir(self):
        _dir = FilterIntDir(
            post_evalue=f"{self.dir.file}/meme_evalue_screened.txt",
            post_entropy=f"{self.dir.file}/meme_entropy_screened.txt",
            post_corr=f"{self.dir.file}/meme_correlated_screened.txt",
            mast_shortseq=f"{self.dir.file}/mast_single",
            mast_postcorr=f"{self.dir.file}/mast_nocorr",
            cluster_pkl=f"{self.dir.file}/cluster_final.pkl")
        return _dir

    def screen_evalue(self):
        # Input: self.dir.memefile
        # Output: self.dir.memefile
        assert os.path.isfile(self.dir.memefile)
        pssm_obj = PSSM_meme(filename=self.dir.memefile)
        to_delete = []
        evalues = pssm_obj.get_evalue()
        for i, evalue in enumerate(evalues):
            if evalue > self.dir.evalue_threshold:
                to_delete.append(i)
        pssm_obj.delete_pssm(to_delete)
        pssm_obj.output()
        if __debug__:
            pssm_obj.output(self._dir.post_evalue)
        assert os.path.isfile(self.dir.memefile)
        return

    def screen_entropy(self):
        # Input: self.dir.memefile
        # Output: self.dir.memefile
        assert os.path.isfile(self.dir.memefile)
        pssm_obj = PSSM_meme(filename=self.dir.memefile)
        to_delete = []
        entropy_bits = pssm_obj.get_entropy_bits()
        for i, entropy in enumerate(entropy_bits):
            if entropy < self.dir.entropy_bits_threshold:
                to_delete.append(i)
        pssm_obj.delete_pssm(to_delete)
        pssm_obj.output()
        if __debug__:
            pssm_obj.output(self._dir.post_entropy)
        assert os.path.isfile(self.dir.memefile)
        return

    def screen_correlated(self):
        # Input: self.dir.memefile | self.dir.short_seq
        # Output: self.dir.memefile | self._dir.mast_shortseq
        assert os.path.isfile(self.dir.memefile)
        assert os.path.isfile(self.dir.short_seq)
        if os.path.isdir(self._dir.mast_shortseq):
            shutil.rmtree(self._dir.mast_shortseq)
        command = f"{self.dir.meme_dir}/mast -remcorr " \
                  f"{self.dir.memefile} {self.dir.short_seq} -o " \
                  f"{self._dir.mast_shortseq}"
        subprocess.run(command, shell=True, executable=self.dir.bash_exec)

        to_delete = get_correlated_motifs(f"{self._dir.mast_shortseq}/mast.txt")
        pssm_obj = PSSM_meme(filename=self.dir.memefile)
        pssm_obj.delete_pssm(to_delete)
        pssm_obj.output()
        if __debug__:
            pssm_obj.output(self._dir.post_corr)
        assert os.path.isdir(self._dir.mast_shortseq)
        assert os.path.isfile(self.dir.memefile)
        return

    def screen_non_combi(self):
        # Input: self.dir.memefile | self.dir.input_seqs
        # Output: self.dir.memefile | self._dir.mast_postcorr
        assert os.path.isfile(self.dir.memefile)
        assert os.path.isfile(self.dir.input_seqs)
        if os.path.isdir(self._dir.mast_postcorr):
            shutil.rmtree(self._dir.mast_postcorr, ignore_errors=True)
        command = f"{self.dir.meme_dir}/mast -remcorr " \
                  f"{self.dir.memefile} {self.dir.input_seqs} -o " \
                  f"{self._dir.mast_postcorr}"
        subprocess.run(command, shell=True, executable=self.dir.bash_exec)
        cluster_dir = Directory.cluster_dir._replace(
            input_mast=f"{self._dir.mast_postcorr}/mast.txt",
            input_meme=self.dir.memefile,
            cluster_pkl=self._dir.cluster_pkl)
        Cluster(cluster_dir).run()

        with open(self._dir.cluster_pkl, 'rb') as file:
            cluster_df = pickle.load(file)
        centroids = cluster_df['centroid']
        profiles_to_keep = set()
        for centroid in centroids:
            for profile in centroid:
                profiles_to_keep.add(profile)
        pssm_obj = PSSM_meme(filename=self.dir.memefile)
        pssm_obj.keep_pssm(profiles_to_keep)
        pssm_obj.output()
        assert os.path.isfile(self.dir.memefile)
        return

    def delete_intermediate(self):
        for file in self._dir:
            self.to_trash(file)
        return

    def to_trash(self, file):
        return move_replace(file, self.dir.trash)


def get_correlated_motifs(fname):
    to_remove = []
    with open(fname, 'r') as file:
        for line in file:
            if re.search("Removed motifs", line):
                motifs1 = re.findall("([0-9]+)\, ", line)
                motifs2 = list(re.findall("([0-9]+) and ([0-9]+)", line)[0])
                to_remove = list(int(i) for i in (motifs1 + motifs2))
                break
    return to_remove