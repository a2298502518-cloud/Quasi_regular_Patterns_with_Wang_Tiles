"""只检查当前源、相容场和统一显示，不继续维护退出路线的测试。"""
from pathlib import Path
import json
import sys
import unittest

import numpy as np

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from qrp_source import expression_jet
from qrp_motif import core_probe
from qrp_checks import audit
from qrp_render import render
from wang_qrp import CornerPhaseAtlas


class CurrentPipelineTests(unittest.TestCase):
    def test_source_gradient(self):
        for q,model in ((5.5,'basic'),(4.8,'cubic-directions')):
            x,y,step=.371,.613,1e-6
            jet=expression_jet(q,x,y,model)
            finite=np.array([(expression_jet(q,x+step,y,model)[0]-expression_jet(q,x-step,y,model)[0])/(2*step),
                             (expression_jet(q,x,y+step,model)[0]-expression_jet(q,x,y-step,model)[0])/(2*step)])
            np.testing.assert_allclose(jet[1:],finite,atol=1e-8,rtol=0)

    def test_current_seams_and_core(self):
        case=dict(q=7.,model='basic',frequency=12.,shift=[0.,0.])
        probe=core_probe(case,6.)
        atlas=CornerPhaseAtlas(probe,case)
        report=audit(atlas,case,probe)
        self.assertLess(report['edge_jet_error'],1e-9)
        self.assertLess(report['protected_core_jet_error'],1e-9)
        self.assertLessEqual(report['geometry_selection']['analytic_relative_wave_vector_bound'],.5)

    def test_direct_cache_and_extension(self):
        case=dict(q=7.,model='basic',frequency=12.,shift=[0.,0.])
        atlas=CornerPhaseAtlas(core_probe(case,6.),case)
        style=json.loads((ROOT/'tools/qrp_wang_defaults.json').read_text(encoding='utf8'))['render']
        plan=atlas.plan(6,11)
        cached=render(atlas,[plan],case,16,style)[0]
        direct=render(atlas,[plan],case,16,style,cached=False)[0]
        np.testing.assert_array_equal(cached,direct)
        extended=atlas.plan(12,11)
        np.testing.assert_array_equal(plan,extended[:6,:6])
        large=render(atlas,[extended],case,16,style)[0]
        np.testing.assert_array_equal(cached,large.crop((0,96,96,192)))


if __name__=='__main__':
    unittest.main()
