
#include "TFile.h"
#include "TTree.h"
#include "TChain.h"
#include "TCut.h"
#include "TF1.h"
#include "TH1F.h"
#include "TCanvas.h"
#include "TString.h"
#include "TH2F.h"

#include "math.h"
#include "iostream.h"
#include "fstream.h"
#include "iomanip.h"
#include "stdio.h"
#include "string.h"
#include "stdlib.h"
#include "vector.h"

#include "PtrelSolver.h"
#include "analysis.h"

ClassImp(PtrelSolver)





/**************************************************************************
 *
 *  access histograms in "inputfilename:/dir/" (2-D histograms)
 *  defined by "histname + tag"
 *  & calculate the tagging efficiency with the fit to corresponding pdf
 *  defined by "pdfbase + bin index".
 *  results are saved in "outfilename"
 *  
 *
 **************************************************************************/
void PtrelSolver::measure(const char *inputfilename, const char *dir, const char *outfilename, const char *tag, const char *histname, int pdfbase, bool sys,
			  const char *mcfilename, const char *mcdir) {

  TH1F *hist =0, *hist_tag=0,  *eff_sys=0;
  TH2F *hist2=0, *hist2_tag=0;
  TH1F *eff =0;
  TF1  *thePdf= 0;
  TF1  *thePdf_tag= 0;
  TF1  *thePdf_sys= 0;
  TF1  *thePdf_sys_tag= 0;
  int   nbins;
  std::vector<double> result, result_sys;

  TString name_base(tag); name_base+="_"; name_base+=histname;

  TString eff_name_sys, eff_name; 
  TString name, tag_name; // to access the tagged histogram


  //  input and output files.
  inputfile = new TFile(inputfilename, "READ");
  outfile   = new TFile(outfilename,   "UPDATE");
  char data_name[100];


  c1->cd();
  name.Resize(0); name.Append(dir); name.Append("/"); name.Append(histname);
  hist2 = (TH2F *)inputfile->Get(name.Data());

  tag_name.Resize(0); 
  tag_name.Append(histname); tag_name.Insert(1, "tag"); 
  tag_name.Append("_"); tag_name.Append(tag);
  tag_name.Insert(0, "/"); tag_name.Insert(0, dir);
  hist2_tag = (TH2F *)inputfile->Get(tag_name.Data());

  std::cout << "information: " << "accessing histogram ... " << name.Data() << std::endl;
  std::cout << "information: " << "accessing histogram ... " << tag_name.Data() << std::endl;
  if (!hist2 || !hist2_tag) {

    std::cout << "information: error in access histograms " << std::endl;
    return;
  }

  eff_name.Resize(0);
  eff_name += name_base;
  eff = (TH1F *)hist2->ProjectionX(eff_name.Data());

  eff_name_sys.Append(eff_name.Data());
  eff_name_sys.Append("_sys");
  eff_sys = (TH1F *)hist2->ProjectionX(eff_name_sys.Data());


  nbins = eff->GetNbinsX();
  int total_num_plots = nbins * 2;
  total_num_plots = (int) (sqrt(total_num_plots) + 1);
  TCanvas *c2 = new TCanvas("c2", "", 900, 900);
  c2->Divide(total_num_plots, total_num_plots);


  for (int ii = 0; ii <= 5; ii++) {
    //  for (int ii = 0; ii <= nbins; ii++) {

    c1->cd();
    thePdf = getPdfByIndex(           pdfbase * ii );
    thePdf_tag = getTaggedPdfByIndex( pdfbase * ii );
    if (!thePdf || !thePdf_tag) {
      std::cout << "can't locate the correct PDF for this data set ..." 
		<< std::endl;
      break;
    } 

    // before tag
    sprintf(data_name, "%s_%d", name_base.Data(), ii);
    if (ii !=0) {

      hist = (TH1F *)hist2->ProjectionY(data_name, ii, ii);
    } else 
      hist = (TH1F *)hist2->ProjectionY(data_name, 1, nbins);


    // after tag
    sprintf(data_name, "%s_%d_tag", name_base.Data(), ii);
    if (ii !=0) {
     
      hist_tag = (TH1F *)hist2_tag->ProjectionY(data_name, ii, ii);
    }    else 
      hist_tag = (TH1F *)hist2_tag->ProjectionY(data_name, 1, nbins);

    effCal(hist, hist_tag, thePdf, thePdf_tag,  &result);


    c2->cd(2*ii+1); hist->Draw("HIST");     thePdf->Draw("SAME");
    c2->cd(2*ii+2); hist_tag->Draw("HIST"); thePdf_tag->Draw("SAME");
    c1->cd();


    // estimate systematic from some fitting
    if (sys) {

      thePdf_sys     = getPdfByIndex( pdfbase*ii, "sys" );
      thePdf_sys_tag = getPdfByIndex( pdfbase*ii, "sys_tag" );
      if (thePdf_sys && thePdf_sys_tag) {

	effCal(hist, hist_tag, thePdf_sys, thePdf_sys_tag,  &result_sys);
	result_sys[1] = sqrt(pow(result[1], 2) + pow(result_sys[0] - result[0] , 2));

	thePdf_sys    ->SetLineColor(kRed);
	thePdf_sys_tag->SetLineColor(kRed);
	c2->cd(2*ii+1); thePdf_sys->Draw("SAME");
	c2->cd(2*ii+2); thePdf_sys_tag->Draw("SAME");
	c1->cd();


	eff_sys->SetBinContent(ii, result[0]);
	eff_sys->SetBinError(ii, result_sys[1]); // only the error is different.
      } else {
	
	std::cout << "information: no systematics applied !" << std::endl;
      }
    }


    if (ii == 0) {
      std::cout <<  "information: tagger[ " << name_base.Data() << "] average efficiency " 
		<< result[0] <<"+/-" 
		<< result[1] <<"(stat.)+/-";

      if (sys)  std::cout <<result_sys[1] << "(total)" << std::endl;
      else std::cout << std::endl;
      continue;
    }

    eff->SetBinContent(ii, result[0]);
    eff->SetBinError(  ii, result[1]);

    outfile->cd();
    hist->Write();
    hist_tag->Write();
  }


  TString epsname(name_base); epsname+="_fits.eps";
  c2->SaveAs(epsname.Data());
  c2->Close();

  TH1F *eff_mc = 0;
  if (mcfilename) {

    if ( !strcmp(mcfilename, inputfilename) ) eff_mc = this->getMCeff(inputfile, mcdir, histname, tag);
    else  eff_mc = this->getMCeff(inputfilename, mcdir, histname, tag);
  }

  outfile->cd();
  eff->Write();
  eff_sys->Write();
  if (eff_mc) {
    eff_mc->Write();

    // make histograms.
    //
    TString title;
    TLegend *leg = 0;
    c1->cd();

    eff->SetMarkerStyle(20);
    eff->SetMinimum(0);
    eff->SetMaximum(1.35);
    if (pdfbase == PT_BASE) formatHist1(eff, "p_{T} (GeV)", "Efficiency");
    if (pdfbase == ETA_BASE) formatHist1(eff, "Pseudorapidity", "Efficiency");
    eff_sys->SetLineColor(kBlack);
    eff_sys->SetMarkerColor(kBlack);

    const char *tmp = saveAsEPS(c1, eff, "PE1");

    eff_mc->SetLineColor(kRed);
    eff_mc->SetMarkerSize(0);
    leg = new TLegend(0.25, 0.77, 0.6, 0.92);
    title.Append(tag);
    leg->SetHeader(title.Data());
    leg->AddEntry(eff, "P_{T}^{rel} Fit", "pl");
    leg->AddEntry(eff_mc, "MC Truth", "l");
    leg->SetFillColor(0);
    leg->SetFillColor(0);
    leg->Draw();
    c1->cd();
    eff_mc->Draw("SAMEHISTE1");
    if (sys) eff_sys->Draw("SAMEPE1");
    c1->SaveAs(tmp);
  }

  c1->Clear();
  outfile->Write();
  outfile->Close();
  inputfile->Close();
}




void PtrelSolver::make(bool sys) {
  



  makeTemplates(5, "/uscms_data/d1/lpcbtag/yumiceva/PerformanceMeasurements/2007_Oct_9_13X/histogramsV4/results_MuBBbarCCbar_TCL_AwayTCL.root", "b_flavor_notag.data", "b_pdf_notag.root", "no_tag", "ppT", "pEta");
  // makeTemplates(5, "/uscms_data/d1/lpcbtag/yumiceva/PerformanceMeasurements/2007_Oct_9_13X/histogramsV4/results_MuBBbarCCbar_TCM_AwayTCL.root", "b_flavor_TCM.data", "b_pdf_TCM.root", "TCM", "pcmbpT", "pcmbEta");

  //  makeTemplates(4, "/uscms_data/d1/lpcbtag/yumiceva/PerformanceMeasurements/2007_Oct_9_13X/histogramsV4/results_MuBBbarCCbar_TCL_AwayTCL.root", "c_flavor_notag.data", "c_pdf_notag.root", "no_tag", "ppT", "pEta");
  //  makeTemplates(4, "/uscms_data/d1/lpcbtag/yumiceva/PerformanceMeasurements/2007_Oct_9_13X/histogramsV4/results_MuBBbarCCbar_TCM_AwayTCL.root", "c_flavor_TCM.data", "c_pdf_TCM.root", "TCM", "pcmbpT", "pcmbEta");
}


void PtrelSolver::produceAll(const char *datafile, const char *dir, const char *outputfile, bool sys) {
  

  initPdfs("./templates/b_flavor_TCM.data", "./templates/c_flavor_TCM.data");
  initPdfs("./templates/b_flavor_TCM.data", "./templates/c_flavor_TCM.data", &combined_pdfs_tag,"tag" );


  measure(datafile, dir, outputfile, "TCL",  "n_pT",  PT_BASE,  sys, datafile, dir);
  measure(datafile, dir, outputfile, "TCL",  "n_eta", ETA_BASE, sys, datafile, dir);
  

  measure(datafile, dir, outputfile, "TCM",  "n_pT",  PT_BASE,  sys, datafile, dir);
  measure(datafile, dir, outputfile, "TCM",  "n_eta", ETA_BASE, sys, datafile, dir);


  measure(datafile, dir, outputfile, "TCT",  "n_pT",  PT_BASE,  sys, datafile, dir);
  measure(datafile, dir, outputfile, "TCT",  "n_eta", ETA_BASE, sys, datafile, dir);

}

