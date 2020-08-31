import ROOT
from   ROOT import TFile
from   ROOT import gDirectory
import sys


def GetKeyNames( self, dir = "" ):
    '''some magic to get all directories within dir'''
    self.cd(dir)
    return [key.GetName() for key in gDirectory.GetListOfKeys()]
TFile.GetKeyNames = GetKeyNames


class HistogramFile(object):
    '''
    basic interface to a root file
    '''
    def __init__(self, filename):
        self.filename = filename

    def __enter__(self):
        '''http://effbot.org/zone/python-with-statement.htm'''
        self.file = ROOT.TFile.Open(self.filename, 'read')
        return self

    def __exit__(self, exception_type, exception_value, traceback):
        self.file.Close()

    def get_histogram(self, name):
        """Return the histogram identified by name from the file.
        """
        # The TFile::Get() method returns a pointer to an object stored in a ROOT file.
        hist = self.file.Get(name)
        if hist:
            return hist
        else:
            raise RuntimeError('Unable to retrieve histogram named {0} from {1}'.format(name, self.filename))







# this below is a lottle demo, lots of crap / needs to be exported and organised
hisN='ana/0_lay1_-11_-6/0_lay1_-11_-6_busycounts'
hisNtwo='ana/0_lay1_-11_-5/0_lay1_-11_-5_busycounts'
def run_demo(outFile, 
             # inFileName = '/data/franzoni/ttbar_D49_1120pre1_PU200_eolupdate_qua_20200723_thr0p5_thrbxm12p5_flfalse.root'
             inFileName = '/eos/home-f/franzoni/www/CMS/HGCSample/SiOptim/2020-08-21-nofold/ttbar_D49_1120pre1_PU200_eolupdate_qua_20200723_thr0p5_thrbxm12p5_flfalse.root'
         ):
    with HistogramFile( inFileName ) as f:
        hist_1 = f.get_histogram(hisN)
        hist_2 = f.get_histogram(hisNtwo)
        # print(hist_1.GetName())
        # print(hist_1.GetEntries())
        # how to write a hitogram to ad different file than it's been red from
        outFile.cd()
        outFile.mkdir('ana/0_lay1_-11_-6/')
        outFile.cd('ana/0_lay1_-11_-6/')
        hist_1.Write()
        outFile.mkdir('ana/0_lay1_-11_-5/')
        outFile.cd('ana/0_lay1_-11_-5/')
        hist_2.Write()
        hist_1.Add(hist_2)
        hist_3=hist_1.Clone()
        hist_3.SetName('puppa')
        outFile.mkdir('ana/SUM/')
        outFile.cd('ana/SUM/')
        hist_3.Write()


#
def rot(waferU, waferV):
    ''' rotate a (U,V) 
    pair by 60 degrees, clock-wise
     rotation by 60 degrees (skype on April 27, 2020 - 14:48)
        U' = u -v
        V' = u
    '''

    u_old = waferU
    v_old = waferV

    waferU = u_old - v_old;
    waferV = u_old;
    
    return waferU,waferV

#
def rotate(subdet, waferU, waferV):
    ''' 
    chose wether to rotate 
    by one sextant (if EE, 60 deg)
    or by one thirdtant (if HE, 120 deg)
    '''
    assert subdet in [0,1]
    
    if   subdet==0:    # 60 degrees in EE
        waferU,waferV = rot(waferU,waferV)
    elif subdet==1: # 120 = x2 60 degrees in HE
        waferU,waferV = rot(waferU,waferV)
        waferU,waferV = rot(waferU,waferV)
    else:
        pass

    return waferU,waferV


#
def isFirstSextant(waferU, waferV):
    ''' true if wafer in first sextant'''
    # condition to be in the first sextant
    if (waferV >= 0 and waferU >waferV) :
        return True
    return False


#
def isFirstThirdtant(waferU, waferV):
    ''' true if wafer in first thirdtant'''

    if isFirstSextant(waferU, waferV):
        return True

    waferU, waferV  = rot(waferU, waferV)
    # if wafer is in 1st sextant after rotating by 60deg => it's in 1st thirdant
    if isFirstSextant(waferU, waferV):
        return True

    return False


#
def remapUV(subdet, waferU, waferV):
    '''
    rotate an UV pair far enogh
    to map it into the first sextant/thirdtant (in EE/EH, resp.)
    '''
    assert subdet in [0,1]

    if      subdet==0:    # EE: rotate to first sextant
        while not isFirstSextant(waferU, waferV):
            waferU,waferV = rotate(subdet,waferU,waferV)
    
    elif subdet==1:     # //HE: rotate to first third-tant
        while not isFirstThirdtant(waferU, waferV):
            waferU,waferV = rotate(subdet,waferU,waferV)

    else:
        pass

    return waferU,waferV



def split_key(ll):
    return map(lambda s: s.split('_'), ll)


import pandas as pd
def keys_to_df(lol):
    uu = pd.DataFrame(lol, columns=['det','lay','U','V'])
    return uu[:-6]

def create_keys_df(keyList):
    return keys_to_df( split_key(keyList) )