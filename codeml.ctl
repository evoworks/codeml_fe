      seqfile = 37flag_region_B.data   * sequence data filename
     treefile = tree.txt      * tree structure file name
      outfile = results.txt   * main result file name

        noisy = 3      * 0,1,2,3,9: how much rubbish on the screen
      verbose = 1      * 1:detailed output
      runmode = 0      * 0:user defined tree

      seqtype = 1      * 1:codons
    CodonFreq = 3      * 0:equal, 1:F1X4, 2:F3X4, 3:F61

        model = 0      * 0:one omega ratio for all branches

      NSsites = 0      * 0:one omega ratio (M0 in Tables 2 and 4)
                       * 1:neutral (M1 in Tables 2 and 4)
                       * 2:selection (M2 in Tables 2 and 4)
                       * 3:discrete (M3 in Tables 2 and 4)
                       * 7:beta (M7 in Tables 2 and 4)
                       * 8:beta&w; (M8 in Tables 2 and 4)

        icode = 0      * 0:universal code

    fix_kappa = 0      * 1:kappa fixed, 0:kappa to be estimated
        kappa = 2      * initial or fixed kappa

    fix_omega = 0      * 1:omega fixed, 0:omega to be estimated 
        omega = 0.8    * initial omega

        Mgene = 4

        ncatG = 3   * # of categories in dG of NSsites models

        getSE = 0   * 0: don't want them, 1: want S.E.s of estimates

   Small_Diff = 3e-7
    cleandata = 0  * remove sites with ambiguity data (1:yes, 0:no)?
     *  method = 1  * 0: simultaneous; 1: one branch at a time
