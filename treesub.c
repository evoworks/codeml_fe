/* TREESUB.c
   subroutines that operates on trees, inserted into other programs 
   such as baseml, basemlg, codeml, and pamp.
*/

extern char BASEs[], nBASEs[], *EquateNUC[], AAs[], BINs[];
extern int noisy;

#ifdef  BASEML
#define REALSEQUENCE
#define NODESTRUCTURE
#define TREESEARCH
#define LSDISTANCE
#define LFUNCTIONS
#define RECONSTRUCTION
#define MINIMIZATION
#endif

#ifdef  CODEML
#define REALSEQUENCE
#define NODESTRUCTURE
#define TREESEARCH
#define LSDISTANCE
#define LFUNCTIONS
#define RECONSTRUCTION
#define MINIMIZATION
#endif

#ifdef  BASEMLG
#define REALSEQUENCE
#define NODESTRUCTURE
#define LSDISTANCE
#endif

#ifdef  RECONSTRUCTION
#define PARSIMONY
#endif

#ifdef  EVOLVE
#define BIRTHDEATH
#endif 

#ifdef  MCMCTREE
#define REALSEQUENCE
#define NODESTRUCTURE
#define LFUNCTIONS
#endif

#define EqPartition(p1,p2,ns) (p1==p2||p1+p2+1==(1<<ns))

#ifdef REALSEQUENCE

int PopEmptyLines (FILE* fseq, int lline, char line[])
{
/* pop out empty lines in the sequence data file.
   returns -1 if EOF.
*/
   char *eqdel=".-?", *p;
   int i;

   for (i=0; ;i++) {
      p=fgets (line, lline, fseq);
      if (p==NULL) return(-1);
      while (*p) 
         if (*p==eqdel[0] || *p==eqdel[1] || *p==eqdel[2] || isalpha(*p)) 
            return(0);
         else p++;
   }
}

int hasbase (char *str)
{
   char *p=str, *eqdel=".-?";
   while (*p) 
      if (*p==eqdel[0] || *p==eqdel[1] || *p==eqdel[2] || isalpha(*p++)) 
         return(1);
   return(0);
}

int blankline (char *str)
{
   char *p=str;
   while (*p) if (isalnum(*p++)) return(0);
   return(1);
}


int GetSeqFileType(FILE *fseq, int *paupseq);
int IdenticalSeqs(void);

int GetSeqFileType(FILE *fseq, int *paupseq)
{
/* paupstart="begin data" and paupend="matrix" identify paup seq files.
   Modify if necessary.
*/
   int  lline=1000;
   char line[1000], *paupstart="begin data",*paupend="matrix", *p;
   char *ntax="ntax",*nchar="nchar";

   if(fscanf(fseq,"%d%d",&com.ns,&com.ls)==2) 
      { *paupseq=0; return(0); }
   *paupseq=1;
   printf("\nseq file is not paml/phylip format.  Trying nexus format.");

   for ( ; ; ) {
      if(fgets(line,lline,fseq)==NULL) error2("seq err1: EOF");
      strcase(line,0);
      if(strstr(line,paupstart)) break;
   }
   for ( ; ; ) {
      if(fgets(line,lline,fseq)==NULL) error2("seq err2: EOF");
      strcase(line,0);
      if((p=strstr(line,ntax))!=NULL) {
         while (*p != '=') { if(*p==0) error2("seq err"); p++; }
         sscanf(p+1,"%d", &com.ns);
         if((p=strstr(line,nchar))==NULL) error2("expect nchar");
         while (*p != '=') { if(*p==0) error2("expect ="); p++; }
         sscanf(p+1,"%d", &com.ls);
         break;
      } 
   }
   /* printf("\nns: %d\tls: %d\n", com.ns, com.ls);  */
   for ( ; ; ) {
      if(fgets(line,lline,fseq)==NULL) error2("seq err1: EOF");
      strcase(line,0);
      if (strstr(line,paupend)) break;
   }
   return(0);
}

int PopupComment(FILE *fseq)
{
   int ch, comment1=']';
   for( ; ; ) {
      ch=fgetc(fseq);
      if(ch==EOF) error2("expecting ]");
      if(ch==comment1) break;
      if(noisy) putchar(ch);
   }
   return(0);
}

int ReadSeq (FILE *fout, FILE *fseq)
{
/* read in sequence, translate into protein (CODON2AAseq), and 
   This counts ngene but does not initialize lgene[].
   It also codes (transforms) the sequences.
   com.seqtype: 0=nucleotides; 1=codons; 2:AAs; 3:CODON2AAs; 4:BINs
   com.pose[] is used to store gene or site-partition labels.
   ls/3 gene marks for codon sequences.
   char opt_c[]="AKGI";
      A:alpha given. K:kappa given
      G:many genes,  I:interlaved format
   lcode: # of characters in the final sequence 
*/
   char *p,*p1, eq='.', comment0='[', *line;
   int i,j,k, ch, noptline=0, lspname=LSPNAME, miss=0, paupseq;
   int lline=10000,lt[NS], igroup, lcode, Sequential=1,basecoding=0;
   int n31=(com.seqtype==CODONseq||com.seqtype==CODON2AAseq?3:1);
   int gap=(n31==3?3:10), nchar=(com.seqtype==AAseq?20:4);
   int h,b[3]={0};
   char *pch=((com.seqtype<=1||com.seqtype==CODON2AAseq)?BASEs:(com.seqtype==2?AAs:BINs));
   char str[4]="   ";

   str[0]=0; h=-1; b[0]=-1; /* avoid warning */
   if (com.seqtype==4) error2("seqtype==BINs, check with author");
   if (noisy>=9 && (com.seqtype<=CODONseq||com.seqtype==CODON2AAseq)) {
      puts("\n\nAmbiguity character definition table:\n");
      FOR (i,(int)strlen(BASEs)) {
         printf("%c (%d): ", BASEs[i],nBASEs[i]);
         FOR(j,nBASEs[i])  printf("%c ", EquateNUC[i][j]);
         FPN(F0);
      }
   }
   GetSeqFileType(fseq, &paupseq);

   if (com.ns>NS) error2("too many sequences.. raise NS?");
   if (com.ls%n31!=0) {
      printf ("\n%d nucleotides, not a multiple of 3!", com.ls); exit(-1);
   }
   lcode=com.ls/n31;
   if (noisy) printf ("\nns = %d  \tls = %d\n", com.ns, com.ls);

   FOR(j,com.ns) {
      if(com.spname[j]) free(com.spname[j]);
      com.spname[j]=(char*)malloc((lspname+1)*sizeof(char));
      FOR(i,lspname+1) com.spname[j][i]=0;
      if (com.z[j]) free(com.z[j]);
      if((com.z[j]=(char*)malloc(com.ls*sizeof(char)))==NULL) error2("oom z");
   }
   com.rgene[0]=1;   com.ngene=1;  
   lline=max2(lline, com.ls/n31*(n31+1)+lspname+50);
   if((line=(char*)malloc(lline*sizeof(char)))==NULL) error2("oom line");
   com.pose=(int*)realloc(com.pose, lcode*sizeof(int));
   if(com.pose==NULL) error2("oom pose");
   FOR(j,lcode) com.pose[j]=0;      /* gene #1, default */

   if(paupseq) goto readseq;

   /* first line */
   if (!fgets(line,lline,fseq)) error2("ReadSeq: first line");
   for (j=0; j<lline && line[j] && line[j]!='\n'; j++) {
      if (!isalnum(line[j])) continue;
      line[j]=(char)toupper(line[j]);
      switch (line[j]) {
         case 'G': noptline++;   break;
         case 'C': basecoding=1; break;
         case 'S': Sequential=1; break;
         case 'I': Sequential=0; break;
         default : 
            printf ("\nBad option '%c' in first line of seqfile\n", line[j]);
            exit (-1);
      }
   }
   if (strchr(line,'C')) {   /* protein-coding DNA sequences */
      if(com.seqtype==2) error2("option C?");
      else if (com.seqtype==0) {
         if (com.ls%3!=0 || noptline<1)  error2("option C?");
         com.ngene=3; FOR(i,3) com.lgene[i]=com.ls/3;
#if(defined(BASEML) || defined(BASEMLG))
         com.coding=1;
         for (i=0;i<com.ls;i++) com.pose[i]=(char)(i%3);
#endif
      }
      noptline--;
   }

   /* option lines */
   FOR (j, noptline) {
      for(ch=0; ; ) {
         ch=(char)fgetc(fseq);
         if(ch==comment0) 
            PopupComment(fseq);
         if(isalnum(ch)) break;
      }

      ch=(char)toupper(ch);
      switch (ch) {
      case ('G') :
         if(basecoding) error2("incorrect option format, use GC?\n");
         if (fscanf(fseq,"%d",&com.ngene)!=1) error2("expecting #gene here..");
         if (com.ngene>NGENE) error2("raise NGENE?");

         fgets(line,lline,fseq);
         if (!blankline(line)) {    /* #sites in each gene on the 2nd line */
            for (i=0,p=line; i<com.ngene; i++) {
               while (*p && !isalnum(*p)) p++;
               if (sscanf(p,"%d",&com.lgene[i])!=1) break;
               while (*p && isalnum(*p)) p++;
            }
            /* if ngene is large and some lgene is on the next line */
            for (; i<com.ngene; i++)
               if (fscanf(fseq,"%d", &com.lgene[i])!=1) error2("EOF at lgene");
            /*
            if(!fgets(line,lline,fseq)) 
               error2("sequence file, gene lengths");
            */
            for(i=0,k=0; i<com.ngene; k+=com.lgene[i],i++)
               FOR(j,com.lgene[i])
                  com.pose[k+j]=i;

            for (i=0,k=0; i<com.ngene; i++) k+=com.lgene[i];
            if (k!=lcode) {
               matIout(F0, com.lgene, 1, com.ngene);
               printf("\n%6d != %d", lcode, k);
               puts("total length over genes is not equal to sequence length");
               if(com.seqtype==1) {
                  puts("Note: gene length is in number of codons.\nSequence length in number of nucleotides");
                  puts("Sequence length in number of nucleotides.");
               }
               exit(-1);
            }
         }
         else {                   /* site marks on later line(s)  */
            for(k=0; k<lcode;) {
               if (com.ngene>9)  fscanf(fseq,"%d", &ch);
               else {
                  do ch=fgetc(fseq); while (!isdigit(ch));
                  ch=ch-(int)'1'+1;  /* assumes 1,2,...,9 are consecutive */
               }
               if (ch<1 || ch>com.ngene)
                  { printf("\ngene mark %d at %d?\n", ch, k+1);  exit (-1); }
               com.pose[k++]=ch-1;
            }
            if(!fgets(line,lline,fseq)) error2("sequence file, gene marks");
         }
         break;
      default :
         printf ("Bad option '%c' in option lines in seqfile\n", line[0]);
         exit (-1);
      }
   }

   readseq:
   /* read sequence */
   if (Sequential)  {    /* sequential */
      if (noisy) printf ("Reading sequences, sequential format..\n");
      FOR (j,com.ns) {
         lspname=LSPNAME;
         FOR (i, 2*lspname) line[i]='\0';
         if (!fgets (line, lline, fseq)) error2("EOF?");
         if (blankline(line)) {
            if (PopEmptyLines (fseq, lline, line))
               { printf("err: empty line (seq %d)\n",j+1); exit(-1); }
         }
         p=line+(line[0]=='=' || line[0]=='>') ;
         while(isspace(*p)) p++;
         if ((ch=strstr(p,"  ")-p)<lspname && ch>0) lspname=ch;
         strncpy (com.spname[j], p, lspname);
         k=strlen(com.spname[j]);
         p+=(k<lspname?k:lspname);

         for (; k>0; k--) /* trim spaces */
            if (!isgraph(com.spname[j][k]))   com.spname[j][k]=0;
            else    break;

         if (noisy>=2) printf ("Reading seq #%2d: %s     \n", j+1, com.spname[j]);
         for (k=0; k<com.ls; p++) {
            while (*p=='\n' || *p=='\0') {
               p=fgets(line, lline, fseq);
               if(p==NULL)
                  { printf("\nEOF at site %d, seq %d\n", k+1,j+1); exit(-1); }
            }
            *p=(char)toupper(*p);
            if((com.seqtype==BASEseq || com.seqtype==CODONseq) && *p=='U') 
               *p='T';
            p1=strchr(pch,*p);
            if (p1 && p1-pch>=nchar)  
               miss=1;
            if (*p==eq) {
               if (j==0) error2(". in 1st seq.?");
               com.z[j][k]=com.z[0][k];  k++;
            }
            else if (p1) 
               com.z[j][k++]=*p;
            else if (isalpha(*p)) {
               printf("\nerr: %c at %d seq %d.",*p,k+1,j+1); 
               puts("\nDid you separate the sequence from its name by 2 spaces?");
               exit(0); 
            }
            else if (*p==(char)EOF) error2("EOF?");
         }           /* for(k) */
         if(strchr(p,'\n')==NULL) /* pop up line return */
            while((ch=fgetc(fseq))!='\n' && ch!=EOF) ;
      }   /* for (j,com.ns) */
   }
   else { /* interlaved */
      if (noisy) printf ("Reading sequences, interlaved format..\n");
      FOR (j, com.ns) lt[j]=0;  /* temporary seq length */
      for (igroup=0; ; igroup++) {
         /*
         printf ("\nreading block %d ", igroup+1);  matIout(F0,lt,1,com.ns);*/

         FOR (j, com.ns) if (lt[j]<com.ls) break;
         if (j==com.ns) break;
         FOR (j,com.ns) {
            if (!fgets(line,lline,fseq)) {
               printf("\nerr reading site %d, seq %d group %d\nsites read in each seq:",
                  lt[j]+1,j+1,igroup+1);
               error2("EOF?");
            }
            if (!hasbase(line)) {
               if (j) {
                  printf ("\n%d, seq %d group %d", lt[j]+1, j+1, igroup+1);
                  error2("empty line.");
               }
               else 
                  if (PopEmptyLines(fseq,lline,line)==-1) {
                     printf ("\n%d, seq %d group %d", lt[j]+1, j+1, igroup+1);
                     error2("EOF?");
                  }
            }
            p=line;
            if (igroup==0) {
               lspname=LSPNAME;
               while(isspace(*p)) p++;
               if ((ch=strstr(p,"  ")-p)<lspname && ch>0) lspname=ch;
               strncpy (com.spname[j], p, lspname);
               k=strlen(com.spname[j]);
               p+=(k<lspname?k:lspname);

               for (; k>0; k--)   /* trim spaces */
                  if (!isgraph(com.spname[j][k]))  com.spname[j][k]=0;
                  else   break;
               if(noisy>=2) printf("Reading seq #%2d: %s     \r",j+1,com.spname[j]);
            }
            for (; *p && *p!='\n'; p++) {
               if (lt[j]==com.ls) break;
               *p=(char)toupper(*p);
               if((com.seqtype==BASEseq || com.seqtype==CODONseq) && *p=='U') 
                  *p='T';
               p1=strchr(pch,*p);
               if (p1 && p1-pch>=nchar) 
                  miss=1;
               if (*p==eq) {
                  if (j==0) {
                     printf("err: . in 1st seq, group %d.\n",igroup);
                     exit (-1);
                  }
                  com.z[j][lt[j]] = com.z[0][lt[j]];
                  lt[j]++;
               }
               else if (p1)
                  com.z[j][lt[j]++]=*p;
               else if (isalpha(*p)) {
                  printf("\nerr:%c at %d seq %d block %d.",
                          *p,lt[j]+1,j+1,igroup+1);
                  exit(-1);
               }
               else if (*p==(char)EOF) error2("EOF");
            }         /* for (*p) */
         }            /* for (j,com.ns) */

         if(noisy>2) {
            printf("\nblock %3d:", igroup+1);
            for(j=0;j<com.ns;j++) printf(" %6d",lt[j]);
         }

      }               /* for (igroup) */
   }
   free(line);

   if(!miss) com.cleandata=1;
   else if (com.cleandata) {  /* remove ambiguity characters */
      if(noisy>2)  puts("\nSites with gaps or missing data are removed.");
      if(fout) {
         fprintf(fout,"\nBefore deleting alignment gaps. %d sites\n",com.ls);
         printsma(fout,com.spname,com.z,com.ns,com.ls,com.ls,gap,com.seqtype,0,0,NULL);
      }
      RemoveIndel ();
      lcode=com.ls/n31;
      if(fout) fprintf(fout,"\nAfter deleting gaps. %d sites\n",com.ls);
   }
   if(fout) 
      printsma(fout,com.spname,com.z,com.ns,com.ls,com.ls,gap,com.seqtype,0,0,NULL);

   if(n31==3) com.ls=lcode;

   /* IdenticalSeqs(); */

#ifdef CODEML
   if(com.seqtype==1 && com.verbose) Get4foldSites();

   if(com.seqtype==CODON2AAseq) {
      if (noisy>2) puts("\nTranslating into AA sequences\n");
      FOR(j,com.ns) {
         if (noisy>2) printf("Translating sequence %d\n",j+1);
         DNA2protein(com.z[j], com.z[j], com.ls,com.icode);
      }
      com.seqtype=AAseq;

      if(fout) {
         fputs("\nTranslated AA Sequences\n",fout);
         fprintf(fout,"%4d  %6d",com.ns,com.ls);
         printsma(fout,com.spname,com.z,com.ns,com.ls,com.ls,10,com.seqtype,0,0,NULL);
      }
   }
#endif

#if (defined CODEML || defined BASEML)
   if(com.ngene==1 && com.Mgene==1) com.Mgene=0;
   if(com.ngene>1 && com.Mgene==1 && com.verbose)  printSeqsMgenes ();

   if(com.bootstrap) { BootstrapSeq("boot.txt");  exit(0); }
#endif

   if(com.cleandata)
      EncodeSeqs();

   if(noisy>=2) printf ("\nSequences read..\n");
   if(com.ls==0)  error2("no sites. Got nothing to do");
   return (0);
}


void EncodeSeqs (void)
{
/* This encodes sequences if com.cleandata==1.
   It uses com.ls.
*/
   int j,h,k, b[3];
   char str[4]="   ";

   FOR(j,com.ns) 
      if(transform(com.z[j],com.ls*(com.seqtype==1?3:1),1,com.seqtype))
         error2("when coding data.");
#ifdef CODEML
   if(com.seqtype==1) {
      FOR(j,com.ns) {
         FOR(h,com.ls) {
            b[0]=com.z[j][h*3]; b[1]=com.z[j][h*3+1]; b[2]=com.z[j][h*3+2];
            if(FROM64[k=b[0]*16+b[1]*4+b[2]]==-1) {
               printf("\ncodon %s in seq. %d is stop (icode=%d)\n", 
                  getcodon(str,k),j+1,com.icode);
               exit(-1);
            }
            com.z[j][h]=(char)FROM64[k];
         }
         if(com.ls>10000) com.z[j]=(char*)realloc(com.z[j],com.ls);
      }
   }
#endif
}



int IdenticalSeqs(void)
{
/* This checks for identical sequences and create a data set of unique 
   sequences.  The file name is <SeqDataFile.unique.  This is casually 
   written and need more testing.
   The routine is called right after the sequence data are read.
   For codon sequences, com.ls has the number of codons, which are NOT
   coded.
*/
   char tmpf[96], keep[NS];
   FILE *ftmp;
   int is,js,h, same,nkept=com.ns;
   int ls1=com.ls*(com.seqtype==CODONseq||com.seqtype==CODON2AAseq?3:1);

   puts("\nIdenticalSeqs: not tested\a");
   FOR(is,com.ns) keep[is]=1;
   FOR(is,com.ns)  { 
      if(!keep[is]) continue;
      FOR(js,is) {
         for(h=0,same=1; h<ls1; h++)
            if(com.z[is][h]!=com.z[js][h]) break;
         if(h==ls1) {
            printf("Seqs. %3d & %3d (%s & %s) are identical!\n",
               js+1,is+1,com.spname[js],com.spname[is]);
            keep[is]=0;
         }
      }
   }
   FOR(is,com.ns) if(!keep[is]) nkept--;
   if(nkept<com.ns) {
      strcpy(tmpf,com.seqf);  strcat(tmpf,".unique");
      if((ftmp=fopen(tmpf,"w"))==NULL) error2("IdenticalSeqs: file error");
      printSeqs(ftmp,NULL,keep,1);
      fclose(ftmp);
      printf("\nUnique sequences collected in %s.\n",tmpf);
   }
   return(0);
}


#if(defined(BASEML) || defined(CODEML) || defined(MML))

void AllPatterns (void)
{
/* This puts all the site patterns into com.z, for calculating the site 
   pattern probabilities under a model.
*/
   int j,h, it,ic;
   char codon[4]="   ";

   printf("Number of sequences? ");
   /* scanf("%d", &com.ns);
   */
   com.ns=5;
   for(j=0,com.npatt=1; j<com.ns; j++) com.npatt*=com.ncode;
   printf ("%3d species, %d site patterns\n", com.ns, com.npatt);
   com.cleandata=1;
   FOR(j,com.ns) {
      com.spname[j]=(char*)realloc(com.spname[j], 11*sizeof(char));
      sprintf(com.spname[j],"Seq.%d ", j+1);
   }
   com.fpatt=(double*) malloc(com.npatt*sizeof(double));
   FOR(j,com.ns) com.z[j]=(char*) malloc(com.npatt*sizeof(char));
   if (com.fpatt==NULL || com.z[com.ns-1]==NULL) error2 ("oom in AllPatterns");
   com.ngene=1;  com.posG[0]=0; com.posG[1]=com.npatt;  com.rgene[0]=1;
   for (h=0; h<com.npatt; h++) {
      for (j=0,it=h,com.fpatt[h]=1; j<com.ns; j++) {
         ic=it%com.ncode; it/=com.ncode;
         com.z[com.ns-1-j][h]=(char)ic;
      }
/*
      FOR(j,com.ns) {
         getcodon(codon, FROM61[com.z[j][h]]);
         printf("%3c ", codon);
      }
      FPN(F0);
*/
   }
   com.ls=com.lgene[0]=com.npatt;
}


void ReadPatternFreq (FILE* fout, char* fpattfile)
{
/* This reads site pattern frequencies as data instead of sequence alignment.
   It uses com.seqtype.  It allows for ambiguity characters.
*/
   FILE *fpattf;
   int h, i,j, ch;
   int n31=(com.seqtype==CODONseq||com.seqtype==CODON2AAseq?3:1);
   char line[10000], *p;
   char *pch=((com.seqtype<=1||com.seqtype==CODON2AAseq)?BASEs:AAs);
   int miss=0, nchar=(com.seqtype==AAseq?20:4);

   if (com.readpattf==2) {
      AllPatterns ();
      com.print=-9;
      return ;
   }
   com.print=0;
   printf ("\n\nRead site-pattern frequencies from %s\n", fpattfile);
   fprintf (fout, "\nSite-pattern frequencies are read from %s\n", fpattfile);
   fpattf=gfopen(fpattfile,"r");
   fscanf (fpattf, "%d%d", &com.ns, &com.npatt);
   printf ("%3d species, %d site patterns\n", com.ns, com.npatt);
   FOR(j,com.ns) {
      com.spname[j]=(char*)realloc(com.spname[j], 11*sizeof(char));
      sprintf(com.spname[j],"Seq.%d ", j+1);
   }
   com.fpatt=(double*) malloc(com.npatt*sizeof(double));
   FOR(j,com.ns) com.z[j]=(char*) malloc(com.npatt*n31*sizeof(char));
   if (com.fpatt==NULL || com.z[com.ns-1]==NULL) error2 ("oom in ReadPatternFreq");
   com.ngene=1;  com.posG[0]=0; com.posG[1]=com.npatt;  com.rgene[0]=1;

   for (h=0; h<com.npatt; h++) {
      for (j=0; j<com.ns; j++) {
         for(i=0; i<n31; i++) {
            ch=fgetc(fpattf);
            while (ch!=EOF && isspace(ch)) ch=fgetc(fpattf);
            if(ch==EOF) error2("patternfreq file: EOF?");

            p=strchr(pch,ch);
            if (p && p-pch>=nchar) miss=1;
            com.z[j][h*n31+i]=(char)ch;
         }
      }
      if(fscanf(fpattf,"%lf",&com.fpatt[h]) != 1) 
         error2("ReadPatternFreq, when reading freq");
      fgets(line,10000,fpattf);
   }
   fclose (fpattf);

   com.cleandata=(char)!miss;
   com.ls=com.npatt;
   if(com.cleandata) EncodeSeqs();

   com.ls=com.lgene[0]=(int)(0.5+sum(com.fpatt,com.npatt));
   if(com.ls<1) puts("\aSuggest recale all freqs to make them larger.");
   printf("\nsum of freqs = %.6f.\n", sum(com.fpatt,com.npatt));
   printf("Sequence length set to %6d.\n", com.ls);
}

#endif


int PatternWeight (FILE *fout)
{
/* This collaps sites into patterns, for nucleotide or amino acid sequences, 
   or transformed codon sequences.  
   zt[] (space) is temporary and is copied back to com.z[].
   poset[] is temprary, needed if com.ngene>1.
   com.pose[i] takes value from 0 to npatt and maps sites to patterns (i=0,...,ls)
*/
   int *fpatti, lst=com.ls,h,ht,j,k=-1,newpatt,ig, *poset;
   int gap=(com.seqtype==CODONseq?3:10);
   int nb=(com.seqtype==1&&!com.cleandata ? 3 : 1);
   char *zt[NS], b1[NS],b3[NS][3], miss[]="-?"; /* b[][] data at site h */
   double nc = (com.seqtype == 1 ? 64 : com.ncode) + !com.cleandata+1;

   if(noisy) printf("Counting site patterns..\n");
   if(com.fpatt) free(com.fpatt);
   if((poset=(int*)malloc(com.ls*sizeof(int)))==NULL) error2("oom poset");
   if((com.seqtype==1 && com.ns<5) || (com.seqtype!=1 && com.ns<7))
      lst = (int)(pow(nc, (double)com.ns)+0.5);
   if(lst>com.ls) lst=com.ls;
   else
      lst=min2(lst*com.ngene,com.ls);
   if((fpatti=(int*)malloc(lst*sizeof(int)))==NULL) 
      error2("oom fpatti");
   for(j=0;j<com.ns;j++) 
      if((zt[j]=(char*)malloc(lst*nb*sizeof(char)))==NULL) error2("oom zt");

   FOR (h,lst) fpatti[h]=0;
   FOR (ig, com.ngene) com.lgene[ig]=0;

   for(ig=0,com.npatt=0; ig<com.ngene; ig++) {
      com.posG[ig]=com.npatt;
      for(h=0; h<com.ls; h++) {
         if(com.pose[h]!=ig) continue;
         if(nb==3) {
            FOR(j,com.ns) FOR(k,nb) b3[j][k]=com.z[j][h*nb+k];
            FOR(j,com.ns) FOR(k,nb) 
               if(b3[j][k]!=miss[0] || b3[j][k]!=miss[1]) goto NotAllMiss3;
            NotAllMiss3:
            if(noisy>=9 && j==com.ns && k==nb)
               puts("Some sites have no data (no harm).");
            for(ht=com.posG[ig],newpatt=1; ht<com.npatt; ht++) {
               FOR(j,com.ns) FOR(k,nb)
                  if(b3[j][k]!=zt[j][ht*nb+k]) goto CheckNextSite3;
               poset[h]=ht;
               newpatt=0; break;    /* h has same data as ht */
               CheckNextSite3: ;
            }
         }
         else {
            FOR(j,com.ns) b1[j]=com.z[j][h];
            FOR(j,com.ns)
               if(b1[j]!=miss[0] || b1[j]!=miss[1]) break;
            if(noisy>=9 && j==com.ns) 
               puts("Some sites have no data (no harm).");

            for(ht=com.posG[ig],newpatt=1; ht<com.npatt; ht++) {
               FOR(j,com.ns) 
                  if(b1[j]!=zt[j][ht]) goto CheckNextSite1;
               poset[h]=ht; 
               newpatt=0; break;    /* h has same data as ht */
               CheckNextSite1: ;
            }
         }
         com.lgene[ig]++;
         if(newpatt) {   /* h has a new pattern */
            if(nb==3) FOR(j,com.ns) FOR(k,nb) zt[j][com.npatt*nb+k]=b3[j][k];
            else      FOR(j,com.ns) zt[j][com.npatt]=b1[j];
            ht=com.npatt++;
            poset[h]=ht;
         }
         if(com.npatt>lst) {
            printf("npatt = %d > lst = %d.. Contact author.", com.npatt,lst);
            exit(-1);
         }
         fpatti[ht]++;
         if(noisy && ((h+1)%100000==0 || h+1==com.ls))
            printf("\r%d site patterns at %d sites", com.npatt,h+1);
      }     /* for (h)  */
   }        /* for (ig) */
   if(noisy) FPN(F0);
 
   if(com.seqtype==CODONseq && com.ngene==10 &&com.lgene[0]==com.ls/3) {
      puts("\nCheck the G option in data file? (Enter)\n");
      getchar();
   }
   com.posG[com.ngene]=com.npatt;
   for(j=1; j<com.ngene; j++) com.lgene[j]+=com.lgene[j-1];

   if(com.lgene[com.ngene-1]!=com.ls) 
      { puts("\algene vs. ls, missing data"); }

   com.fpatt=(double*)malloc(com.npatt*sizeof(double));
   for(h=0;h<com.npatt;h++) com.fpatt[h]=fpatti[h];
   free(fpatti);
   FOR(j,com.ns) {
      free(com.z[j]);
      com.z[j]=(char*)realloc(zt[j],com.npatt*nb*sizeof(char));
   }
   memcpy(com.pose, poset,com.ls*sizeof(int));
   free(poset);

   if (fout) {
      fprintf (fout, "\nns = %d  \tls = %d", com.ns,com.ls);
      if (com.ngene>1) {
         fprintf (fout,"\nngene =%3d, lengths =", com.ngene);
         FOR(j,com.ngene)
            fprintf (fout,"%7d", (j?com.lgene[j]-com.lgene[j-1]:com.lgene[j]));
         fprintf(fout,"\n  Starting at pattern");
         FOR(j,com.ngene) fprintf(fout,"%7d", com.posG[j]+1);
      }
      fprintf(fout,"\n# site patterns = %d\n", com.npatt);
      FOR (h,com.npatt) {
         fprintf(fout," %4.0f", com.fpatt[h]);
         if((h+1)%15==0) FPN(fout);
      }
      FPN(fout);
      if(com.seqtype==1 && com.cleandata) {
#if(defined CODEML)
         printsmaCodon (fout,com.z,com.ns,com.npatt,com.npatt,1);
#endif
      }
      else {
         newpatt=com.npatt*nb;
         printsma(fout,com.spname,com.z,com.ns,newpatt,newpatt,gap,
            com.seqtype,com.cleandata,1,NULL);
      }
      FPN(fout);
   }
   return (0);
}



void AddFreqSeqGene(int js,int ig,double pi0[],double pi[],int *missing);

int Initialize (FILE *fout)
{
/* Count site patterns (com.fpatt) and calculate base or amino acid frequencies
   in genes and species.  This works on raw (uncoded) data.  
   Ambiguity characters in sequences are resolved by iteration. 
   For frequencies in each species, they are resolved within that sequence.
   For average base frequencies among species, they are resolved over all 
   species.

   This routine is called by baseml and aaml.  codonml uses another
   routine InitializeCodon()
*/
   char *pch=(com.seqtype==0?BASEs:(com.seqtype==2?AAs:BINs)), indel[]="-?";
   int wname=30, h,js,k, ig, nconstp, nc=com.ncode;
   int irf,nrf=20, miss=0;
   double pi0[20], t,lmax=0;

#if (defined(BASEML) || defined(CODEML) || defined(BASEMLG) || defined(MCMCTREE) || defined(PAMP) || defined(MML))
   if(!com.readpattf) PatternWeight(fout);
#endif

   if(noisy) puts("Counting frequencies..");
   if(fout)  fprintf(fout,"\nFrequencies..");
   for(h=0,nconstp=0; h<com.npatt; h++) {
      for (js=1;js<com.ns;js++)  if(com.z[js][h]!=com.z[0][h]) break;
      if (js==com.ns && com.z[0][h]!=indel[0] && com.z[0][h]!=indel[1])  
         nconstp+=(int)com.fpatt[h];
   }
   for (ig=0,zero(com.pi,nc); ig<com.ngene; ig++) {
      if (com.ngene>1)  fprintf (fout,"\n\nGene %2d (len %4d)",
                             ig+1, com.lgene[ig]-(ig==0?0:com.lgene[ig-1]));
      fprintf(fout,"\n%*s",wname, ""); FOR(k,nc) fprintf(fout,"%7c",pch[k]);

      /* The following block calculates freqs in each species for each gene.  
         Ambiguities are resolved in each species.  com.pi and com.piG are 
         used for output only, and are not be used later with missing data.
      */
      zero(com.piG[ig],nc);
      for(js=0; js<com.ns; js++) {
         FOR(k,nc) pi0[k]=1./nc;
         for(irf=0; irf<nrf; irf++) {
            zero(com.pi,nc);
            AddFreqSeqGene(js, ig, pi0, com.pi, &miss);
            t=sum(com.pi,nc);
            if(t<1e-10) 
               puts("empty sequences?");
            abyx(1/t, com.pi, nc);
            t=distance(com.pi,pi0,nc);
            if((t=distance(com.pi,pi0,nc))<1e-8) 
               break;
            xtoy(com.pi,pi0,nc);
         }   /* for(irf) */
         fprintf(fout,"\n%-*s",wname,com.spname[js]);
         FOR(k,nc) fprintf(fout,"%7.4f",com.pi[k]);
         FOR(k,nc) com.piG[ig][k]+=com.pi[k]/com.ns;
      }    /* for(js,ns) */
      if(com.ngene>1) {
         fprintf(fout,"\n\n%-*s",wname,"Mean");
         FOR(k,nc) fprintf(fout,"%7.4f",com.piG[ig][k]);
      }
   }

   /* If there are missing data, the following block calculates freqs 
      in each gene for com.piG[], as well com.pi[] for the entire sequence.  
      Ambiguities are resolved over entire data sets across species (within 
      each gene for com.piG[]).  These are used in ML calculation later.
   */
   if(!miss) {
      for (ig=0,zero(com.pi,nc); ig<com.ngene; ig++) {
         t=(ig==0?com.lgene[0]:com.lgene[ig]-com.lgene[ig-1])/(double)com.ls;
         FOR(k,nc)  com.pi[k]+=com.piG[ig][k]*t;
      }
   }
   else {
      for (ig=0; ig<com.ngene; ig++) { 
         xtoy(com.piG[ig],pi0,nc);
         for(irf=0; irf<nrf; irf++) {  /* com.piG[] */
            zero(com.piG[ig],nc);
            FOR(js,com.ns)
               AddFreqSeqGene(js, ig, pi0, com.piG[ig], &miss);
            t=sum(com.piG[ig],nc);
            if(t<1e-10) 
               puts("empty sequences?");
            abyx(1/t, com.piG[ig], nc);
            if(distance(com.piG[ig],pi0,nc)<1e-8) break;
            xtoy(com.piG[ig],pi0,nc);
         }         /* for(irf) */
      }            /* for(ig) */
      zero(pi0,nc);
      FOR(k,nc) FOR(ig,com.ngene) pi0[k]+=com.piG[ig][k]/com.ngene;
      for(irf=0; irf<nrf; irf++) {  /* com.pi[] */
         zero(com.pi,nc);
         FOR(ig, com.ngene)  FOR(js,com.ns)
            AddFreqSeqGene(js, ig, pi0, com.pi, &miss);
         abyx(1/sum(com.pi,nc), com.pi, nc);
         if(distance(com.pi,pi0,nc)<1e-8) break;
         xtoy(com.pi,pi0,nc);
      }            /* for(ig) */
   }
   fprintf (fout, "\n\n%-*s", wname, "Average");
   FOR (k,nc) fprintf(fout,"%7.4f", com.pi[k]);
   if(miss) fputs("\n(Ambiguity characters are used to calculate freqs.)\n",fout);

   fprintf (fout,"\n\n# constant sites: %6d (%.2f%%)",
            nconstp, (double)nconstp*100./com.ls);

   if (com.model==0 || (com.seqtype==BASEseq && com.model==1)) {
      fillxc(com.pi, 1./nc, nc);
      FOR(ig,com.ngene) xtoy (com.pi, com.piG[ig], nc);
   }
   if (com.seqtype==BASEseq && com.model==5) { /* T92 model */
      com.pi[0]=com.pi[2]=(com.pi[0]+com.pi[2])/2;
      com.pi[1]=com.pi[3]=(com.pi[1]+com.pi[3])/2;
      FOR(ig,com.ngene) {
         com.piG[ig][0]=com.piG[ig][2]=(com.piG[ig][0]+com.piG[ig][2])/2;
         com.piG[ig][1]=com.piG[ig][3]=(com.piG[ig][1]+com.piG[ig][3])/2;
      }
   }

   /* this is used only for REV & REVu in baseml and model==3 in aaml */
   if(com.seqtype==AAseq) {
      for (k=0,t=0; k<nc; k++) t+=(com.pi[k]>0);
      if (t<=4)  puts("\n\a\t\tAre these a.a. sequences?");
   }
   if(!miss && com.ngene==1) {
      for(h=0,lmax=-(double)com.ls*log((double)com.ls); h<com.npatt; h++)
         if(com.fpatt[h]>1) lmax+=com.fpatt[h]*log((double)com.fpatt[h]);
   }
   if(fout) {
      if(lmax) fprintf(fout, "\nln Lmax (unconstrained) = %.6f\n", lmax);
      fflush(fout);
   }

   return(0);
}


void AddFreqSeqGene(int js, int ig, double pi0[], double pi[], int *missing)
{
/* This adds the character counts in sequence js in gene ig to pi, 
   using pi0, by resolving ambiguities
   The data are coded if com.cleandata==1.
*/
   char *pch=(com.seqtype==0?BASEs:(com.seqtype==2?AAs:BINs));
   int k, h, b, nc=com.ncode, nb,ib[4];
   double t;

   if(com.cleandata) {
      for(h=com.posG[ig]; h<com.posG[ig+1]; h++) 
         { b=com.z[js][h]; pi[b]+=com.fpatt[h]; }
   }
   else {
      for(h=com.posG[ig]; h<com.posG[ig+1]; h++) {
         b=strchr(pch,com.z[js][h])-pch;
         if(b<nc)
            pi[b]+=com.fpatt[h];
         else {
            *missing=1;
            if(com.seqtype==BASEseq) {
               NucListall(com.z[js][h], &nb, ib);
               for(k=0,t=0; k<nb; k++) t+=pi0[ib[k]];
               FOR(k,nb) pi[ib[k]]+=pi0[ib[k]]/t*com.fpatt[h];
            }
            else if(com.seqtype==AAseq)
               for(k=0;k<20;k++) pi[k]+=pi0[k]*com.fpatt[h];
         }
      }
   }
}




int RemoveIndel(void)
{
/* Remove ambiguity characters and indels in the untranformed sequences, 
   Changing com.ls and com.pose[] (site marks for multiple genes).
   For codonml, com.ls is still 3*#codons
   Called at the end of ReadSeq, when com.pose[] are still site marks.
   All characters in com.z[][] not found in the character string pch are
   considered ambiguity characters and are removed.
*/
   int  h,k, j,js,lnew,nindel, n31,nchar;
   char b, *pch, *miss;  /* miss[h]=1 if site (codon) h is missing, 0 otherwise */

   if(com.seqtype==CODONseq||com.seqtype==CODON2AAseq)
      { n31=3; nchar=4; pch=BASEs; }
   else {
      n31=1;
      if(com.seqtype==AAseq)        { nchar=20; pch=AAs; }
      else if(com.seqtype==BASEseq) { nchar=4; pch=BASEs; }
      else                          { nchar=2; pch=BINs; }
    }

   if (com.ls%n31) error2("ls in RemoveIndel.");
   if((miss=(char*)malloc(com.ls/n31 *sizeof(char)))==NULL)
      error2("oom miss");
   FOR (h,com.ls/n31) miss[h]=0;
   for (js=0; js<com.ns; js++) {
      for (h=0,nindel=0; h<com.ls/n31; h++) {
         for (k=0; k<n31; k++) {
            b=(char)toupper(com.z[js][h*n31+k]);
            FOR(j,nchar) if(b==pch[j]) break;
            if(j==nchar) { miss[h]=1; nindel++; }
         }
      }
      if (noisy>2 && nindel) 
         printf("\n%6d ambiguity characters in seq. %d", nindel,js+1);
   }
   if(noisy>2) {
      for(h=0,k=0; h<com.ls/n31; h++)  if(miss[h]) k++;
      printf("\n%d sites are removed. ", k);
      if(k<1000)
         for(h=0; h<com.ls/n31; h++)  if(miss[h]) printf(" %2d", h+1);
   }

   for (h=0,lnew=0; h<com.ls/n31; h++)  {
      if(miss[h]) continue;
      for (js=0; js<com.ns; js++) {
         for (k=0; k<n31; k++)
            com.z[js][lnew*n31+k]=com.z[js][h*n31+k];
      }
      com.pose[lnew]=com.pose[h];
      lnew++;
   }
   com.ls=lnew*n31;
   free(miss);
   return (0);
}



int MPInformSites (void)
{
/* Outputs parsimony informative and noninformative sites into 
   two files named MPinf.seq and MPninf.seq
   Uses transformed sequences.  
   Not used for a long time.  Does not work if com.pose is NULL.  
*/
   char *imark, *pch=(com.seqtype==0?BASEs:(com.seqtype==2?AAs:BINs));
   int h, i, markb[NS], inf, lsinf;
   FILE *finf, *fninf;

puts("\nMPInformSites: missing data not dealt with yet?\n");

   finf=fopen("MPinf.seq","w");
   fninf=fopen("MPninf.seq","w");
   if (finf==NULL || fninf==NULL) error2("MPInformSites: file creation error");

   puts ("\nSorting parsimony-informative sites: MPinf.seq & MPninf.seq");
   if ((imark=(char*)malloc(com.ls*sizeof(char)))==NULL) error2("oom imark");
   for (h=0,lsinf=0; h<com.ls; h++) {
      for (i=0; i<com.ns; i++) markb[i]=0;
      for (i=0; i<com.ns; i++) markb[(int)com.z[i][com.pose[h]]]++;

      for (i=0,inf=0; i<com.ncode; i++)  if (markb[i]>=2)  inf++;
      if (inf>=2) { imark[h]=1; lsinf++; }
      else imark[h]=0;
   }
   fprintf (finf, "%6d%6d\n", com.ns, lsinf);
   fprintf (fninf, "%6d%6d\n", com.ns, com.ls-lsinf);
   for (i=0; i<com.ns; i++) {
      fprintf (finf, "\n%s\n", com.spname[i]);
      fprintf (fninf, "\n%s\n", com.spname[i]);
      for (h=0; h<com.ls; h++)
         fprintf ((imark[h]?finf:fninf), "%c", pch[(int)com.z[i][com.pose[h]]]);
      FPN (finf); FPN(fninf);
   }
   free (imark);
   fclose(finf);  fclose(fninf);
   return (0);
}


int PatternJC69like (FILE *fout)
{
/* further collaps of site patterns for JC69-like models, called after
   PatternWeight().  Used for nucleotide and amino acid models.
   This works with unclean data as well. (August 2002)
*/
   char zh[NS],b, whole[4]={'-','?','N'};
   int npatt0=com.npatt, h, ht, j,k, same=0, ig, recode;
   char *pch=(com.seqtype==0?BASEs:(com.seqtype==2?AAs:NULL));

   if(com.seqtype==2) whole[2]='\0';
   for (h=0,com.npatt=0,ig=-1; h<npatt0; h++) {
      if (ig<com.ngene-1 && h==com.posG[ig+1]) { com.posG[++ig]=com.npatt; }
      /* copy data to zh, recode if possible */
      if(com.cleandata) { /* always recode */
         zh[0]=b=0; b++;
         for (j=1; j<com.ns; j++) {
            FOR(k,j) if (com.z[j][h]==com.z[k][h]) break;
            zh[j]= (char)(k<j?zh[k]:b++);
         }
      }
      else { /* not recoded if ambiguity characters exist */
         FOR(j,com.ns) zh[j]=com.z[j][h];
         for (j=0,recode=1; j<com.ns; j++) {
            if(strchr(whole, zh[j])) 
               { zh[j]=whole[0]; continue; } /* OK */
            k=strchr(pch,zh[j])-pch;
            if(k<0)
               printf("strange character %c in seq.", zh[j]);
            else if(k<com.ncode) continue;      /* OK */
            recode=0; break;
         }
         if(recode) {
            b=0;
            if(zh[0]!=whole[0]) 
               zh[0]=pch[(int)b++];
            for (j=1; j<com.ns; j++) {
               if(zh[j]==whole[0]) continue;
               FOR(k,j) if (com.z[j][h]==com.z[k][h]) break;
               if(k<j) zh[j]=zh[k];
               else    zh[j]=pch[(int)b++];
            }
         }
      }

      for (ht=com.posG[ig],same=0; ht<com.npatt; ht++) {
         for (j=0,same=1; j<com.ns; j++)
            if (zh[j]!=com.z[j][ht]) { same=0; break; }
         if (same) break; 
      }
      if (same)  com.fpatt[ht]+=com.fpatt[h];
      else {
         FOR (j, com.ns) com.z[j][com.npatt]=zh[j];
         com.fpatt[com.npatt++]=com.fpatt[h];
      }
      FOR(k,com.ls) if (com.pose[k]==h) com.pose[k]=ht;
   }     /* for (h)   */
   com.posG[com.ngene]=com.npatt;
   if (noisy>=2) printf ("\nnew no. site patterns:%7d\n", com.npatt);

   if (fout) {
      fprintf(fout,"\nnew number of site patterns = %d\n", com.npatt);
      FOR (h,com.npatt) {
         fprintf(fout," %4.0f", com.fpatt[h]);
         if((h+1)%15==0) FPN(fout);
      }
/*    for baseml only, to print out the patterns.  comment out. */
      if(com.seqtype==0) {
         fprintf (fout, "\nno. site patterns:%7d\n", com.npatt);
         printsma(fout,com.spname,com.z,com.ns,com.npatt,com.npatt,10,com.seqtype, com.cleandata, 0,NULL);
      }
   }
   return (0);
}

int Site2Pattern (FILE *fout)
{
   int h;
   fprintf(fout,"\n\nMapping site to pattern (i.e. site %d has pattern %d):\n",
      com.ls-1, com.pose[com.ls-2]+1);
   FOR (h, com.ls) {
      fprintf (fout, "%6d", com.pose[h]+1);
      if ((h+1)%10==0) FPN (fout);
   }
   FPN (fout);
   return (0);
}

#endif


int print1seq (FILE*fout, char *z, int ls, int encoded, int pose[])
{
/* This prints out one sequence, which may be coded.  Codon seqs are coded
   as 0,1,2,...60 if called from codeml.c or 0,1,2,...,63 otherwise.
   This may be risking.  Check when use.
   z[] contains patterns if (pose!=NULL)
   This uses com.seqtype.
*/
   int i, h,hp, gap=10;
   char *pch=(com.seqtype==0?BASEs:(com.seqtype==2?AAs:BINs)), str[4]="";
   int nb=(com.seqtype==CODONseq?3:1);


   if(!encoded) {  /* raw data, not coded */
      FOR(h,ls) {
         hp=(pose?pose[h]:h);
         FOR(i,nb) 
            fputc(z[hp*nb+i],fout);
/*
         FOR(i,nb) fputc(z[(pose?pose[h]:h) *nb+i],fout);
*/
         if(com.seqtype==CODONseq || (h+1)%gap==0) fputc(' ',fout);
      }
   }
   else {    /* cleandata, coded */
      FOR(h,ls) {
         hp=(pose?pose[h]:h);
         if(com.seqtype!=CODONseq) 
            fputc(pch[(int)z[hp]],fout);
         else {
#ifdef CODEML
            fprintf(fout,"%s",getcodon(str,FROM61[(int)z[hp]])); /* 0,1,...,60 */
#else
            fprintf(fout,"%s",getcodon(str,z[hp]));         /* 0,1,...,63 */
#endif
         }
         if(com.seqtype==CODONseq || (h+1)%gap==0) fputc(' ',fout);
      }
   }
   return(0);
}

void printSeqs(FILE *fout, int *pose, char keep[], int format)
{
/* Print sequences into fout, using paml (format=0) or paup (format=1) formats.
   Use pose=NULL if called before site patterns are collapsed.  
   keep[] marks the sequences to be printed.  Use NULL for keep if all sequences 
   are to be printed.
   Sequences may (com.cleandata==1) and may not (com.cleandata=0) be coded.
   com.z[] has site patterns if pose!=NULL.
   This uses com.seqtype, and com.ls is the number of codons for codon seqs.
   See notes in print1seq()

   format=0:PAML & PHYLIP format
          1:NEXUS, with help from John Huelsenbeck.  Thanks to John.
*/
   int j,ls1=(com.seqtype==CODONseq?3*com.ls:com.ls), nskept=com.ns, wname=30;
   char *dt=(com.seqtype==AAseq?"protein":"dna");

   if(keep) FOR(j,com.ns) nskept -= !keep[j];
   if (format==1) {  /* NEXUS format */
      fprintf(fout,"\nbegin data;\n");
      fprintf(fout,"   dimensions ntax=%d nchar=%d;\n", nskept,ls1);
      fprintf(fout,"   format datatype=%s missing=? gap=-;\n   matrix\n",dt);
   }
   else    
      fprintf(fout,"%8d %8d\n",nskept,ls1);

   for(j=0; j<com.ns; j++,FPN(fout)) {
      if(keep && !keep[j]) continue;
      fprintf(fout,"%s%-*s  ", (format?"      ":""),wname,com.spname[j]);
      print1seq(fout,com.z[j],com.ls,com.cleandata, pose);
   }
   if (format==1) fprintf(fout, "   ;\nend;");
   FPN(fout);
   fflush(fout);
}

#define gammap(x,alpha) (alpha*(1-pow(x,-1/alpha)))
/* DistanceREV () used to be here, moved to pamp. 
*/

#if (defined BASEML || defined BASEMLG || defined MCMCTREE) 

double SeqDivergence (double x[], int model, double alpha, double *kappa)
{
/* alpha=0 if no gamma 
   return -1 if in error.
   Check DistanceF84() if variances are wanted.
*/
   int i,j;
   double p[4], Y,R, a1,a2,b, P1,P2,Q,fd,tc,ag, GC;
   double small=1e-6,largek=999, larged=9;

   if (testXMat(x))
      error2("X err..");
   for (i=0,fd=1,zero(p,4); i<4; i++) {
      fd -= x[i*4+i];
      FOR (j,4) { p[i]+=x[i*4+j]/2;  p[j]+=x[i*4+j]/2; }
   }
   P1=x[0*4+1]+x[1*4+0];
   P2=x[2*4+3]+x[3*4+2];
   Q = x[0*4+2]+x[0*4+3]+x[1*4+2]+x[1*4+3]+ x[2*4+0]+x[2*4+1]+x[3*4+0]+x[3*4+1];
   if(fd<small/com.ls) fd=0;
   if(P1<small/com.ls) P1=0; 
   if(P2<small/com.ls) P2=0; 
   if(Q<small/com.ls) Q=0;
   Y=p[0]+p[1];    R=p[2]+p[3];  tc=p[0]*p[1]; ag=p[2]*p[3];

   switch (model) {
   case (JC69):
      FOR (i,4) p[i]=.25;
   case (F81):
      for (i=0,b=0; i<4; i++)  b += p[i]*(1-p[i]);
      if (1-fd/b<=0) return (larged);

      if (alpha<=0) return (-b*log (1-fd/b));
      else return  (-b*gammap(1-fd/b,alpha));
   case (K80) :
      a1=1-2*(P1+P2)-Q;   b=1-2*Q;
/*      if (a1<=0 || b<=0) return (-1); */
      if (a1<=0 || b<=0) return (larged);
      if (alpha<=0)  { a1=-log(a1);  b=-log(b); }
      else          { a1=-gammap(a1,alpha); b=-gammap(b,alpha); }
      a1=.5*a1-.25*b;  b=.25*b;
      if(b>small) *kappa = a1/b; else *kappa=largek;
      return (a1+2*b);
   case (F84):
      a1=(2*(tc+ag)+2*(tc*R/Y+ag*Y/R)*(1-Q/(2*Y*R)) -P1-P2) / (2*tc/Y+2*ag/R);
      b = 1 - Q/(2*Y*R);
/*      if (a1<=0 || b<=0) return (-1); */
      if (a1<=0 || b<=0) return (larged);
      if (alpha<=0) { a1=-log(a1); b=-log(b); }
      else          { a1=-gammap(a1,alpha); b=-gammap(b,alpha); }
      a1=.5*a1;  b=.5*b;
      *kappa = a1/b-1;
      *kappa = max2(*kappa, -.5);
      return  4*b*(tc*(1+ *kappa/Y)+ag*(1+ *kappa/R)+Y*R);
   case (HKY85):         /* HKY85, from Rzhetsky & Nei (1995 MBE 12, 131-51) */
      *kappa = largek;
      a1=1-Y*P1/(2*tc)-Q/(2*Y);
      a2=1-R*P2/(2*ag)-Q/(2*R);
      b=1-Q/(2*Y*R);
      if (a1<=0 || a2<=0 || b<=0) return (larged);
      if (alpha<=0) { a1=-log(a1); a2=-log(a2); b=-log(b); }
      else   { a1=-gammap(a1,alpha); a2=-gammap(a2,alpha); b=-gammap(b,alpha);}
      a1 = -R/Y*b + a1/Y;
      a2 = -Y/R*b + a2/R;
      if (b>0) *kappa = min2((a1+a2)/(2*b), largek);
      return 2*(p[0]*p[1] + p[2]*p[3])*(a1+a2)/2 + 2*Y*R*b;
   case (T92):
      *kappa = largek;
      GC=p[1]+p[3];
      a1 = 1 - Q - (P1+P2)/(2*GC*(1-GC));   b=1-2*Q;
      if (a1<=0 || b<=0) return (larged);
      if (alpha<=0) { a1=-log(a1); b=-log(b); }
      else   { a1=-gammap(a1,alpha); b=-gammap(b,alpha);}
      if(Q>0) *kappa = 2*a1/b-1;
      return 2*GC*(1-GC)*a1 + (1-2*GC*(1-GC))/2*b;
   case (TN93):         /* TN93  */
      a1=1-Y*P1/(2*tc)-Q/(2*Y);  
      a2=1-R*P2/(2*ag)-Q/(2*R);
      b=1-Q/(2*Y*R);
/*      if (a1<=0 || a2<=0 || b<=0) return (-1); */
      if (a1<=0 || a2<=0 || b<=0) return (larged);
      if (alpha<=0) { a1=-log(a1); a2=-log(a2); b=-log(b); }
      else   { a1=-gammap(a1,alpha); a2=-gammap(a2,alpha); b=-gammap(b,alpha);}
      a1=.5/Y*(a1-R*b);  a2=.5/R*(a2-Y*b);  b=.5*b;
      *kappa = largek;
      if (b>0) *kappa = min2((a1+a2)/(2*b), largek);
      return 4*p[0]*p[1]*a1 + 4*p[2]*p[3]*a2 + 4*Y*R*b;
   }
   return (-1);
}


double DistanceIJ (int is, int js, int model, double alpha, double *kappa)
{
/* Distance between sequences is and js.
   See DistanceMatNuc() for more details.
*/
   int h, k1,k2, nc=4, missing=0;
   double x[16];

   if(com.cleandata)
      for (h=0,zero(x,16); h<com.npatt; h++)
         x[com.z[is][h]*nc+com.z[js][h]] += com.fpatt[h];
   else 
      for (h=0,zero(x,16); h<com.npatt; h++) {
         for(k1=0;k1<nc;k1++) if(BASEs[k1]==com.z[is][h]) break;
         for(k2=0;k2<nc;k2++) if(BASEs[k2]==com.z[js][h]) break;
         if(k1<nc && k2<nc)   x[k1*nc+k2] += com.fpatt[h];
         else                 missing=1;
      }
   abyx(1./sum(x,16),x,16);
/*
matout(F0, x, 4, 4);
exit(0);
*/
   return SeqDivergence(x, model, alpha, kappa);
}


#if (defined LSDISTANCE && defined REALSEQUENCE)

extern double *SeqDistance;

int DistanceMatNuc (FILE *fout, FILE*f2base, int model, double alpha)
{
/* This calculates pairwise distances.  The data may be clean and coded 
   (com.cleandata==1) or not.  In the latter case, ambiguity sites are not 
   used (pairwise deletion).  Site patterns are used.
*/
   int is,js, status=0;
   double kappat=0, t,bigD=5;
   
   if(f2base) fprintf(f2base,"%6d\n", com.ns);
   if(model>=REV) model=TN93; /* TN93 here */
   if(fout) {
      fprintf(fout,"\nDistances:%5s", models[model]);
      if (model!=JC69 && model!=F81) fprintf (fout, " (kappa) ");
      fprintf(fout," (alpha set at %.2f)\n", alpha);
      fprintf(fout,"This matrix is not used in later m.l. analysis.\n");
      if(!com.cleandata) fputs("\n(Pairwise deletion.)",fout);
   }
   for(is=0; is<com.ns; is++) {
      if(fout) fprintf(fout,"\n%-15s  ", com.spname[is]);
      if(f2base) fprintf(f2base,"%-15s   ", com.spname[is]);
      for(js=0; js<is; js++) {
         t=DistanceIJ(is, js, model, alpha, &kappat);
         if(t<0) { t=bigD; status=-1; }
         SeqDistance[is*(is-1)/2+js] = t;
         if(f2base) fprintf(f2base," %7.4f", t);
         if(fout) fprintf(fout,"%8.4f", t);
         if(fout && (model==K80 || model>=F84))
            fprintf(fout,"(%7.4f)", kappat);
       }
       if(f2base) FPN(f2base);
   }
   if(fout) FPN(fout);
   if(status) puts("\ndistance formula sometimes inapplicable..");
   return(status);
}

#endif


#ifdef BASEMLG
extern int CijkIs0[];
#endif

extern int nR;
extern double Cijk[], Root[];

int RootTN93 (int model, double kappa1, double kappa2, double pi[], 
    double *f, double Root[])
{
   double T=pi[0],C=pi[1],A=pi[2],G=pi[3],Y=T+C,R=A+G;

   if (model==F84) { kappa2=1+kappa1/R; kappa1=1+kappa1/Y; }

   *f=1/(2*T*C*kappa1+2*A*G*kappa2 + 2*Y*R);

   Root[0] = 0;
   Root[1] = - (*f);
   Root[2] = -(Y+R*kappa2) * (*f);
   Root[3] = -(Y*kappa1+R) * (*f);
   return (0);
}


int EigenTN93 (int model, double kappa1, double kappa2, double pi[],
    int *nR, double Root[], double Cijk[])
{
/* initialize Cijk[] & Root[], which are the only part to be changed
   for a new substitution model
   for JC69, K80, F81, F84, HKY85, TN93
   Root: real Root divided by v, the number of nucleotide substitutions.
*/
   int i,j,k, nr;
   double f, U[16],V[16], t;
   double T=pi[0],C=pi[1],A=pi[2],G=pi[3],Y=T+C,R=A+G;

   if (model==JC69 || model==F81) kappa1=kappa2=com.kappa=1; 
   else if (com.model<TN93)       kappa2=kappa1;
   RootTN93 (model, kappa1, kappa2, pi, &f, Root);

   *nR=nr = 2+(model==K80||model>=F84)+(model>=HKY85);
   U[0*4+0]=U[1*4+0]=U[2*4+0]=U[3*4+0]=1;
   U[0*4+1]=U[1*4+1]=1/Y;   U[2*4+1]=U[3*4+1]=-1/R;
   U[0*4+2]=U[1*4+2]=0;  U[2*4+2]=G/R;  U[3*4+2]=-A/R;
   U[2*4+3]=U[3*4+3]=0;  U[0*4+3]=C/Y;  U[1*4+3]=-T/Y;

   xtoy (pi, V, 4);
   V[1*4+0]=R*T;   V[1*4+1]=R*C;
   V[1*4+2]=-Y*A;  V[1*4+3]=-Y*G;
   V[2*4+0]=V[2*4+1]=0;  V[2*4+2]=1;   V[2*4+3]=-1;
   V[3*4+0]=1;  V[3*4+1]=-1;   V[3*4+2]=V[3*4+3]=0;

   FOR (i,4) FOR (j,4) {
      Cijk[i*4*nr+j*nr+0]=U[i*4+0]*V[0*4+j];
      switch (model) {
      case JC69:
      case F81:
         for (k=1,t=0; k<4; k++) t += U[i*4+k]*V[k*4+j];
         Cijk[i*4*nr+j*nr+1] = t;
         break;
      case K80:
      case F84:
         Cijk[i*4*nr+j*nr+1]=U[i*4+1]*V[1*4+j];
         for (k=2,t=0; k<4; k++) t += U[i*4+k]*V[k*4+j];
         Cijk[i*4*nr+j*nr+2]=t;
         break;
      case HKY85:   case T92:   case TN93:
         for (k=1; k<4; k++)  Cijk[i*4*nr+j*nr+k] = U[i*4+k]*V[k*4+j];
         break;
      default:
         error2("model in EigenTN93");
      }
   }
#ifdef BASEMLG
   FOR (i,64) CijkIs0[i] = (Cijk[i]==0);
#endif
   return(0);
}


#endif


#ifdef BASEML

int EigenQREVbase (FILE* fout, double kappa[], 
                   double pi[], int *nR, double Root[], double Cijk[])
{
/* pi[] is constant
*/
   int i,j,k, nr=(com.ngene>1&&com.Mgene>=3?com.nrate/com.ngene:com.nrate);
   double Q[16], U[16], V[16], mr, space_pisqrt[4];

   NPMatUVRoot=0;
   *nR=4;
   zero (Q, 16);
   if(com.model==REV) {
      for(i=0,k=0,Q[3*4+2]=Q[2*4+3]=1; i<3; i++) for (j=i+1; j<4; j++)
         if(i*4+j!=11) Q[i*4+j]=Q[j*4+i]=kappa[k++];
   }
   else       /* (model==REVu) */
      FOR(i,3) for(j=i+1; j<4; j++)
         Q[i*4+j]=Q[j*4+i] = (StepMatrix[i*4+j] ? kappa[StepMatrix[i*4+j]-1] : 1);

   FOR(i,4) FOR(j,4) Q[i*4+j] *= pi[j];

   for (i=0,mr=0; i<4; i++) 
      { Q[i*4+i]=0; Q[i*4+i]=-sum(Q+i*4, 4); mr-=pi[i]*Q[i*4+i]; }
   abyx (1/mr, Q, 16);

   if (fout) {
      mr=2*(pi[0]*Q[0*4+1]+pi[2]*Q[2*4+3]);
      fprintf(fout, "Rate parameters:  ");
      FOR(j,nr) fprintf(fout, " %8.5f", kappa[j]);
      fprintf(fout, "\nBase frequencies: ");
      FOR(j,4) fprintf(fout," %8.5f", pi[j]);
      fprintf (fout, "\nRate matrix Q, Average Ts/Tv =%9.4f", mr/(1-mr));
      matout (fout, Q, 4,4);
   }
   else {
      eigenQREV(Q, pi, 4, Root, U, V, space_pisqrt);
      FOR (i,4) FOR(j,4) FOR(k,4) Cijk[i*4*4+j*4+k] = U[i*4+k]*V[k*4+j];
   }
   return (0);
}


int QUNREST (FILE *fout, double Q[], double rate[], double pi[])
{
/* This constructs the rate matrix Q for the unrestricted model.
   pi[] is changed in the routine.
*/
   int i,j,k;
   double mr, space[20];

   if(com.model==UNREST) {
      for (i=0,k=0,Q[14]=1; i<4; i++) FOR(j,4) 
         if (i!=j && i*4+j != 14)  Q[i*4+j]=rate[k++];
   }
   else  /* (model==UNRESTu) */
      FOR(i,4) FOR(j,4)
         if(i!=j) 
            Q[i*4+j] = (StepMatrix[i*4+j] ? rate[StepMatrix[i*4+j]-1] : 1);

   FOR(i,4)  { Q[i*4+i]=0; Q[i*4+i]=-sum(Q+i*4, 4); }

   /* get pi */

   QtoPi (Q, com.pi, 4, space);

   for (i=0,mr=0; i<4; i++)  mr -= pi[i]*Q[i*4+i];
   for (i=0; i<4*4; i++)  Q[i]/=mr;

   if (fout) {
      mr=pi[0]*Q[0*4+1]+pi[1]*Q[1*4+0]+pi[2]*Q[2*4+3]+pi[3]*Q[3*4+2];

      fprintf(fout, "Rate parameters:  ");
      FOR(j,com.nrate) fprintf(fout, " %8.5f", rate[j]);
      fprintf(fout, "\nBase frequencies: ");
      FOR(j,4) fprintf(fout," %8.5f", pi[j]);
      fprintf (fout,"\nrate matrix Q, Average Ts/Tv (similar to kappa/2) =%9.4f",mr/(1-mr));
      matout (fout, Q, 4, 4);
   }
   return (0);
}

#endif


#ifdef LSDISTANCE

double *SeqDistance=NULL; 
int *ancestor=NULL;

int SetAncestor()
{
/* This finds the most recent common ancestor of species is and js.
*/
   int is, js, it, a1, a2;

   FOR(is,com.ns) FOR(js,is) {
      it=is*(is-1)/2+js;
      ancestor[it]=-1;
      for (a1=is; a1!=-1; a1=nodes[a1].father) {
         for (a2=js; a2!=-1; a2=nodes[a2].father)
            if (a1==a2) { ancestor[it]=a1; break; }
         if (ancestor[it] != -1) break;
      }
      if (ancestor[it] == -1) error2("no ancestor");
   }
   return(0);
}

int fun_LS (double x[], double diff[], int np, int npair);

int fun_LS (double x[], double diff[], int np, int npair)
{
   int i,j, aa, it=-np;
   double dexp;

   if (SetBranch(x) && noisy>2) puts ("branch len.");
   if (npair != com.ns*(com.ns-1)/2) error2("# seq pairs err.");
   FOR(i,com.ns) FOR(j, i) {
      it=i*(i-1)/2+j;
      for (aa=i,dexp=0; aa!=ancestor[it]; aa=nodes[aa].father)
         dexp += nodes[aa].branch;
      for (aa=j; aa!=ancestor[it]; aa=nodes[aa].father)
         dexp += nodes[aa].branch;
      diff[it]=SeqDistance[it]-dexp;
   }
   return(0);
}

int LSDistance (double *ss,double x[],int (*testx)(double x[],int np))
{
/* get Least Squares estimates of branch lengths for a given tree topology
*/
   int i;

   if ((*testx)(x, com.ntime)) {
      matout (F0, x, 1, com.ntime);
      puts ("initial err in LSDistance()");
   }
   SetAncestor();
   i=nls2((com.ntime>20&&noisy>=3?F0:NULL),
      ss,x,com.ntime,fun_LS,NULL,testx,com.ns*(com.ns-1)/2,1e-6);
   return (i);
}

double PairDistanceML(int is, int js)
{
/* This calculates the ML distance between is and js, the sum of ML branch 
   lengths along the path between is and js.
   LSdistance() has to be called once to set ancestor before calling this 
   routine.
*/
   int it, a;
   double dij=0;

   if(is==js) return(0);
   if(is<js) { it=is; is=js; js=it; }

   it=is*(is-1)/2+js;
   for (a=is; a!=ancestor[it]; a=nodes[a].father)
      dij += nodes[a].branch;
   for (a=js; a!=ancestor[it]; a=nodes[a].father)
      dij += nodes[a].branch;
   return(dij);
}


int GroupDistances()
{
/* This calculates average group distances by summing over the ML 
   branch lengths */
   int newancestor=0, i,j, ig,jg;
/*   int ngroup=2, Ningroup[10], group[200]={1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2}; */ /* dloop for HC200.paup */
   int ngroup=10, Ningroup[10], group[115]={
       10, 9, 9, 9, 9, 9, 9, 9, 9, 10, 
       9, 9, 3, 3, 1, 1, 1, 1, 1, 1, 
       1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
       1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
       1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
       1, 2, 2, 2, 2, 2, 2, 4, 4, 4, 
       4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 
       4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 
       5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 
       5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 
       6, 7, 7, 7, 7, 7, 7, 7, 7, 7, 
       8, 8, 8, 8, 8};  /* dloop data for Anne Yoder, ns=115 */
   double dgroup, npairused;

/* ngroup=2; for(j=0;j<com.ns; j++) group[j]=1+(group[j]>2); */

   for(j=0;j<ngroup;j++) Ningroup[j]=0;
   for(j=0;j<com.ns; j++) Ningroup[group[j]-1]++;
   printf("\n# sequences in group:");
   matIout(F0,Ningroup,1,ngroup);
   if(ancestor==NULL) {
      newancestor=1;
      ancestor=(int*)realloc(ancestor, com.ns*(com.ns-1)/2*sizeof(int));
      if(ancestor==NULL) error2("oom ancestor");
   }
   SetAncestor();

   for(ig=0; ig<ngroup; ig++) {
      printf("\ngroup %2d",ig+1);
      for(jg=0; jg<ig+1; jg++) {
         dgroup=0;  npairused=0;
         for(i=0;i<com.ns;i++) for(j=0;j<com.ns;j++) {
            if(i!=j && group[i]==ig+1 && group[j]==jg+1) {
               dgroup += PairDistanceML(i, j);
               npairused++;
            }
         }
         dgroup/=npairused;
         printf("%9.4f", dgroup);

         /* printf("%6.1f", dgroup/0.2604*5); */ /* 0.2604, 0.5611 */
      }
   }
   if(newancestor==1)  free(ancestor);
   return(0);
}

#endif 

#ifdef NODESTRUCTURE

void BranchToNode (void)
{
/* tree.root need to be specified before calling this
*/
   int i, from, to;
   
   tree.nnode=tree.nbranch+1;
   FOR (i,tree.nnode)
      { nodes[i].father=nodes[i].ibranch=-1;  nodes[i].nson=0; }
   FOR (i,tree.nbranch) {
      from=tree.branches[i][0];
      to  =tree.branches[i][1];
      nodes[from].sons[nodes[from].nson++]=to;
      nodes[to].father=from;
      nodes[to].ibranch=i;
   }
}

void NodeToBranchSub (int inode);

void NodeToBranchSub (int inode)
{
   int i, ison;

   FOR (i, nodes[inode].nson) {
      tree.branches[tree.nbranch][0]=inode;
      tree.branches[tree.nbranch][1]=ison=nodes[inode].sons[i];
      nodes[ison].ibranch=tree.nbranch++;
      if(nodes[ison].nson>0)  NodeToBranchSub(ison);
   }
}

void NodeToBranch (void)
{
   tree.nbranch=0;
   NodeToBranchSub (tree.root);
   if(tree.nnode!=tree.nbranch+1)
      error2("nnode != nbranch + 1?");
}


void ClearNode (int inode)
{
/* a source of confusion. Try not to use this routine.
*/
   nodes[inode].father=nodes[inode].ibranch=-1;
   nodes[inode].nson=0;
   nodes[inode].branch=nodes[inode].age=0;
   /* nodes[inode].label=0; */
   /* nodes[inode].branch=0; clear node structure only, not branch lengths */
   /* FOR (i, com.ns) nodes[inode].sons[i]=-1; */
}

int ReadaTreeB (FILE *ftree, int popline)
{
   char line[255];
   int nodemark[NS*2-1]={0}; /* 0: absent; 1: father only (root); 2: son */
   int i,j, state=0, YoungAncestor=0;

if(com.clock) {
   puts("\nbranch representation of tree might not work with clock model.");
   getchar();
}

   fscanf (ftree, "%d", &tree.nbranch);
   FOR (j, tree.nbranch) {
      FOR (i,2) {
         if (fscanf (ftree, "%d", & tree.branches[j][i]) != 1) state=-1;
         tree.branches[j][i]--;
         if(tree.branches[j][i]<0 || tree.branches[j][i]>com.ns*2-1) 
            error2("ReadaTreeB: node numbers out of range");
      }
      nodemark[tree.branches[j][1]]=2;
      if(nodemark[tree.branches[j][0]]!=2) nodemark[tree.branches[j][0]]=1;
      if (tree.branches[j][0]<com.ns)  YoungAncestor=1;

      printf ("\nBranch #%3d: %3d -> %3d",j+1,tree.branches[j][0]+1,tree.branches[j][1]+1);

   }
   if(popline) fgets(line, 254, ftree);
   for(i=0,tree.root=-1; i<tree.nbranch; i++) 
      if(nodemark[tree.branches[i][0]]!=2) tree.root=tree.branches[i][0];
   if(tree.root==-1) error2("root err");
   for(i=0; i<com.ns; i++)
      if(nodemark[i]==0) {
         matIout(F0,nodemark,1,com.ns);
         error2("branch specification of tree");
      }

   if(YoungAncestor) {
      puts("\nAncestors in the data?  Take care.");
      if(!com.cleandata) {
         puts("This kind of tree does not work with unclean data.");
         getchar();
      }
   }

/*
   com.ntime = com.clock ? (tree.nbranch+1)-com.ns+(tree.root<com.ns)
                         : tree.nbranch;
*/

   BranchToNode ();
   return (state);
}


int OutaTreeB (FILE *fout)
{
   int j;
   char *fmt[]={" %3d..%-3d", " %2d..%-2d"};
   FOR (j, tree.nbranch)
      fprintf(fout, fmt[0], tree.branches[j][0]+1,tree.branches[j][1]+1);
   return (0);
}

int GetTreeFileType(FILE *ftree, int *ntree, int *pauptree, int shortform);

int GetTreeFileType(FILE *ftree, int *ntree, int *pauptree, int shortform)
{
/* paupstart="begin trees" and paupend="translate" identify paup tree files.
   paupch=";" will be the last character before the list of trees.
   Modify if necessary.
*/
   int i,k, lline=32000, ch=0, paupch=';';
   char line[32000];
   char *paupstart="begin tree", *paupend="translate";

   *pauptree=0;
   k=fscanf(ftree,"%d%d",&i,ntree);
   if(k==2 && i==com.ns) return(0);                 /* old paml style */
   else if(k==1) { *ntree=i; return(0); }           /* phylip & molphy style */
   while(ch!='(' && !isalnum(ch) && ch!=EOF)  ch=fgetc(ftree);  /* treeview style */
   if(ch=='(') { *ntree=-1; ungetc(ch,ftree); return(0); }

   puts("\n# seqs in tree file does not match.  Read as the nexus format.");
   for ( ; ; ) {
      if(fgets(line,lline,ftree)==NULL) error2("tree err1: EOF");
      strcase(line,0);
      if (strstr(line,paupstart)) { *pauptree=1; *ntree=-1; break; }
   }
   if(shortform) return(0);
   for ( ; ; ) {
      if(fgets(line,lline,ftree)==NULL) error2("tree err2: EOF");
      strcase(line,0);
      if (strstr(line,paupend)) break;
   }
   for ( ; ; ) {
      if((ch=fgetc(ftree))==EOF) error2("tree err3: EOF");
      if (ch==paupch) break;
   }
   if(fgets(line,lline,ftree)==NULL) error2("tree err4: EOF");

   return(0);
}

int PaupTreeRubbish(FILE *ftree);
int PaupTreeRubbish(FILE *ftree)
{
/* This reads out the string in front of the tree in the nexus format, 
   typically "tree PAUP_1 = [&U]" with "[&U]" optional
*/
   int ch;
   char line[10000], *paup1="tree", treename[64], c;

   fscanf(ftree,"%s",line);
   strcase(line,0);
   if (strstr(line,paup1)==NULL) return(-1);
   fscanf(ftree, "%s%s%c", treename, line, &c);

   for (; ;) {
      if((ch=fgetc(ftree))==EOF) { 
         puts("err or end of treefile: expect ]");
         return(-1);
      }
      else if (ch==']') break;
      else if (ch=='(') error2("err treefile: strange");
   }
   return(0);
}


static int *CladeLabel=NULL;

void DownTreeCladeLabel(int inode)
{
/* This goes down the tree to change $ labels (stored in CladeLabel[]) into
   # labels (stored in nodes[].label).  To deal with nested clade labels,
   branches within a clade are labeled by negative numbers initially, and 
   converted to positive labels at the end of the algorithm.
*/
   int i,j, lab=CladeLabel[inode];

   if(nodes[inode].label<=0 && lab) {
      /* find tips that are descendents of inode */
      for(i=0,nodes[inode].label=-lab; i<com.ns; i++) {
         for(j=i; j!=tree.root && j!=inode; j=nodes[j].father)
            ;
         if(j==inode) { /* tip i is a descendent of inode */
            for(j=i; j!=tree.root && j!=inode; j=nodes[j].father)
               if(nodes[j].label<=0) nodes[j].label = -lab;
         }
      }
   }

   for(i=0; i<nodes[inode].nson; i++)
      if(nodes[nodes[inode].sons[i]].nson)
         DownTreeCladeLabel(nodes[inode].sons[i]);

   if(inode==tree.root)
      for(i=0; i<tree.nnode; i++) nodes[i].label=fabs(nodes[i].label);
}

int IsNameNumber(char line[])
{
/* returns 0 if line has species number; 1 if name; 2 if both number and name
*/
   int isname=0, j,k, ns=com.ns;
   int SeparatorFixed=(int)'_';

   if(ns<1) error2("ns=0 in IsNameNumber");
   /* both name and number? */
   k = strchr(line, SeparatorFixed) - line;
   for(j=0; j<k; j++)
      if(!isdigit(line[j])) break;
   if(j==k) 
      isname=2;
   else {
      for(j=0; line[j]; j++)  /* name or number? */
         if(!isdigit(line[j])) { isname=1; break; }  
   }
   if(isname==0 || isname==2) {
      sscanf(line,"%d",&k);
      if(k<1||k>ns) error2("species number outside range.");
   }
   return(isname);
}


int ReadaTreeN (FILE *ftree, int *haslength, int *haslabel, int copyname, int popline)
{
/* Read a tree from ftree, using the parenthesis node representation of trees.
   Branch lengths are read in nodes[].branch, and branch (node) labels 
   (integers) are preceeded by # and read in nodes[].label.  If the clade label
   $ is used, the label is read into CladeLabel[] first and then moved into
   nodes[].label in the routine DownTreeCladeLabel().

   This assumes that com.ns is known.
   Species names are considered case-sensitive, with trailing spaces ignored.

   copyname = 0: species numbers and names are both accepted, but names have 
                 to match the names in com.spname[], which are from the 
                 sequence data file.  Used by baseml and codeml, for example.
              1: species names are copied into com.spname[], but species 
                 numbers are accepted.  Used by evolver for simulation, 
                 in which case no species names were read before.
              2: the tree must have species names, which are copied into com.spname[].
                 Note that com.ns is assumed known.  To remove this restrition, 
                 one has to consider the space for nodes[], CladeLabel, starting 
                 node number etc.

   isname = 0:   species number; 1: species name; 2:both number and name
*/
   int cnode, cfather=-1;  /* current node and father */
   int inodeb=0;  /* node number that will have the next branch length */
   int i,j, level=0, isname, ch=' ', icurspecies=0;
   char check[NS], delimiters[]="(),:#$=@><;", skips[]="\"\'";
   int lline=32000;
   char line[32000];

   if(com.ns<=0)  error2("need to know ns before reading tree.");

   if((CladeLabel=(int*)malloc((com.ns*2-1)*sizeof(int)))==NULL) 
      error2("oom trying to get space for cladelabel");
   FOR(i,2*com.ns-1) CladeLabel[i]=0;

   /* initialization */
   FOR(i,com.ns) check[i]=0;
   *haslength=0, *haslabel=0;
   tree.nnode=com.ns;  tree.nbranch=0;
   FOR(i,2*com.ns-1) {
      nodes[i].father=nodes[i].ibranch=-1;
      nodes[i].nson=0;  nodes[i].label=0;  nodes[i].branch=0;
      nodes[i].age=0;  /* TipDate models set this for each tree later. */
#if (defined(BASEML) || defined(CODEML))
      nodes[i].fossil=0;
#endif
   }
   while(isspace(ch)) ch=fgetc(ftree);  /* skip spaces */
   ungetc(ch,ftree);
   if (isdigit(ch)) { ReadaTreeB(ftree,popline); return(0); }

   for ( ; ; ) {
      ch = fgetc (ftree);
      if (ch==EOF) return(-1);
      else if (!isgraph(ch) || ch==skips[0] || ch==skips[1]) continue;
      else if (ch=='(') {       /* left (  */
         level++;
         cnode=tree.nnode++;
         if(tree.nnode>2*com.ns-1) 
			 error2("check tree: perhaps too many '('?");
         if (cfather>=0) {
            nodes[cfather].sons[nodes[cfather].nson++] = cnode;
            nodes[cnode].father=cfather;
            tree.branches[tree.nbranch][0]=cfather;
            tree.branches[tree.nbranch][1]=cnode;
            nodes[cnode].ibranch=tree.nbranch++;
         }
         else
            tree.root=cnode;
         cfather=cnode;
      }
      /* treating : and > in the same way is risky. */
      else if (ch==')') { level--;  inodeb=cfather; cfather=nodes[cfather].father; }
      else if (ch==':'||ch=='>') { *haslength=1; fscanf(ftree,"%lf",&nodes[inodeb].branch); }
      else if (ch=='#'||ch=='<') { *haslabel=1; fscanf(ftree,"%lf",&nodes[inodeb].label); }
      else if (ch=='$')          { *haslabel=1; fscanf(ftree,"%d",&CladeLabel[inodeb]); }
      else if (ch=='@'||ch=='=') { 
         fscanf(ftree,"%lf",&nodes[inodeb].age);
#if (defined(BASEML) || defined(CODEML))
         if(com.clock) nodes[inodeb].fossil=1;
#endif
      }
      else if (ch==',') ;
      else if (ch==';' && level!=0) error2("; in treefile");
      else { /* read species name or number */
         line[0]=(char)ch;  line[1]=(char)fgetc(ftree);
/*         if(line[1]==(char)EOF) error2("eof in tree file"); */

         for (i=1; i<lline; )  { /* read species name into line[] until delimiter */
            if ((strchr(delimiters,line[i]) && line[i]!='@') 
               || line[i]==(char)EOF || line[i]=='\n')
               { ungetc(line[i],ftree); line[i]=0; break; }
            line[++i]=(char)fgetc(ftree);
         }
         for(j=i-1;j>0;j--) /* trim spaces*/
            if(isgraph(line[j])) break; else line[j]=0;
         isname=IsNameNumber(line);

         if (isname==2) {       /* both number and name */
            sscanf(line, "%d", &cnode);   cnode--;
            strcpy(com.spname[cnode], line);
         }
         else if (isname==0) {  /* number */
            if(copyname==2) error2("Use names in tree.");
            sscanf(line, "%d", &cnode);   cnode--;
         }
         else {                 /* name */
            if(!copyname) {
               for(i=0; i<com.ns; i++) if (!strcmp(line,com.spname[i])) break;
               if((cnode=i)==com.ns) { printf("\nSpecies %s?\n", line); exit(-1); }
            }
            else {
               if(icurspecies>com.ns-1) error2("Something seems wrong with the tree.");
               strcpy(com.spname[cnode=icurspecies++], line);
            }
         }

         nodes[cnode].father=cfather;
         nodes[cfather].sons[nodes[cfather].nson++]=cnode;
         tree.branches[tree.nbranch][0]=cfather;
         tree.branches[tree.nbranch][1]=cnode;
         nodes[cnode].ibranch=tree.nbranch++;
         check[inodeb=cnode]++;
      }
      if (level<=0) break;
   }
   /* read branch length and label for the root if any */
   /* treating : and > in the same way is risky. */
   for ( ; ; ) {
      for(; ;) {
         ch=fgetc(ftree);
         if(ch=='\n' || ch==EOF || (isgraph(ch) && ch!=skips[0] && ch!=skips[1]))
            break;
      }
      if (ch==':'||ch=='>')       fscanf(ftree, "%lf", &nodes[tree.root].branch);
      else if (ch=='#'||ch=='<') { *haslabel=1; fscanf(ftree,"%lf",&nodes[inodeb].label); }
      else if (ch=='$')           error2("why do you label the whole tree?");
      else if (ch=='@'||ch=='=') { 
         fscanf(ftree,"%lf",&nodes[inodeb].age);
#if (defined(BASEML) || defined(CODEML))
         if(com.clock) nodes[inodeb].fossil=1;
#endif
      }
      else if (ch==';')  break;
      else  /* anything unrecognised */
         { ungetc(ch,ftree);  break; }
   }

   if (popline) fgets (line, lline, ftree);
   FOR(i,com.ns) {
      if(check[i]>1) 
         { printf("\nSeq #%d occurs more than once in the tree\n",i+1); exit(-1); }
      else if(check[i]<1) 
         { printf("\nSeq #%d is missing in the tree\n",i+1); exit(-1); }
   }
   if(tree.nbranch>2*com.ns-2) { 
      printf("nbranch %d", tree.nbranch); puts("too many branches in tree?");
   }
   if (tree.nnode != tree.nbranch+1) {
      printf ("\nnnode%6d != nbranch%6d + 1\n", tree.nnode, tree.nbranch);
      exit(-1);
   }

/* check that it is o.k. to comment out this line
   com.ntime = com.clock ? (tree.nbranch+1)-com.ns+(tree.root<com.ns)
                         : tree.nbranch;
*/

#if(defined(BASEML) || defined(CODEML))
   /* check and convert clade labels $ */
   if(com.clock>1 || (com.seqtype==1 && com.model>=2)) {
      for(i=0,j=0; i<tree.nnode; i++) {
         if(i<com.ns && CladeLabel[i]) 
            error2("cannot have $ label for tips.");
         if(CladeLabel[i]) j++;
      }
      if(j) /* number of clade labels */
         DownTreeCladeLabel(tree.root);

      /*
      OutaTreeN(F0,1,2);  FPN(F0);
      */

      for(i=0,com.nbtype=0; i<tree.nnode; i++) { 
         if(i==tree.root) continue;
         j=(int)nodes[i].label;
         if(j+1>com.nbtype)  com.nbtype=j+1;
         if(j<0||j>tree.nbranch-1)  error2("branch label in the tree");
      }
      if (com.nbtype==0)
         error2("need branch labels in the tree for the model.");
      else {
         printf("\n%d branch types are in tree. Stop if wrong.", com.nbtype);
      }

#if(defined(CODEML))
      if(com.seqtype==1 && com.NSsites && (com.model==2 || com.model==3) && com.nbtype!=2)
         error2("only two branch types are allowed for branch models.");
#endif

   }
#endif

   free(CladeLabel);
   return (0);
}




int OutSubTreeN (FILE *fout, int inode, int spnames, int printopt, char *labelfmt);

int OutSubTreeN (FILE *fout, int inode, int spnames, int printopt, char *labelfmt)
{
   int i,ison;

   fputc ('(', fout);
   for(i=0; i<nodes[inode].nson; i++) {
      ison=nodes[inode].sons[i];
      if(nodes[ison].nson==0) {
         if(spnames) {
            if(printopt==2) fprintf(fout, "%d_",ison+1);
            fprintf(fout,"%s",com.spname[ison]);
         }
         else 
            fprintf(fout,"%d",ison+1);
      }
      else
         OutSubTreeN(fout, ison, spnames, printopt, labelfmt);

      if(printopt==2 && nodes[ison].nson) 
         fprintf(fout," %d ",ison+1);
      if((printopt==3 || printopt==5) && nodes[ison].label>0)
         fprintf(fout, labelfmt, nodes[ison].label);
      if((printopt==4 || printopt==5) && nodes[ison].age) 
         fprintf(fout, " @%.3f", nodes[ison].age);


      if(printopt) { 

/*  Add branch labels to be read by Rod Page's TreeView. */
         if(printopt==6) {
#if (defined CODEML)
            fprintf(fout," #%.4f ", nodes[ison].omega);
#elif (defined EVOLVER)
            if(nodes[ison].nodeStr) fprintf(fout," '%s'", nodes[ison].nodeStr);
#endif
         }
         fprintf(fout,": %.6f", nodes[ison].branch);
      }

      if(i<nodes[inode].nson-1) fprintf(fout,", ");
   }
   fputc (')', fout);
   return (0);
}


int OutaTreeN (FILE *fout, int spnames, int printopt)
{
/* printopt >= 1: branch lengths
             = 2: node numbers.
             = 3: labels.
             = 4: ages
             = 5: labels & ages
             = 6: omega (codeml), nodeStr (evolver)
*/
   int i, intlabel=1;
   char* labelfmt[2]={"#%.5f", "#%.0f"};

   if(printopt==3 || printopt==5) {
      for(i=0;i<tree.nnode;i++) 
         if(nodes[i].label-(int)nodes[i].label != 0) intlabel=0;
   }

   OutSubTreeN(fout,tree.root,spnames,printopt, labelfmt[intlabel]);
   if(printopt==2) fprintf(fout," %d ", tree.root+1);
   if((printopt==3 || printopt==5) && nodes[tree.root].label>0) 
      fprintf(fout, labelfmt[intlabel], nodes[tree.root].label);
   if((printopt==4 || printopt==5)  && nodes[tree.root].age) 
      fprintf(fout, " @%.3f", nodes[tree.root].age);

   if(printopt && nodes[tree.root].branch>1e-8)
      fprintf(fout,": %.6f", nodes[tree.root].branch);

   fputc(';',fout);
   return(0);
}


int DeRoot (void)
{
   int i, ison, sib, root=tree.root;

   if(nodes[tree.root].nson!=2) error2("in DeRoot?");

   ison=nodes[root].sons[i=0];
   if(nodes[ison].nson==0) ison=nodes[root].sons[i=1];
   ison=nodes[root].sons[i]; sib=nodes[root].sons[1-i];
   nodes[sib].branch+=nodes[ison].branch;
   nodes[sib].father=tree.root=ison;
   nodes[ison].father=-1;
   nodes[ison].sons[nodes[ison].nson++]=sib;
   nodes[ison].branch=0;
   return(0);
}

int Nsonroot=-1;

int PruneSubTreeN (int inode, int keep[])
{
/* This prunes tips from the tree, using keep[com.ns].  Removed nodes in the 
   big tree has nodes[].father=-1 and nodes[].nson=0.
   Do not change nodes[inode].nson and nodes[inode].sons[] until after the 
   node's descendent nodes are all processed.  So when a son is deleted, 
   only the father node's nson is changed, but not 
*/
   int i,j, ison, father=nodes[inode].father, nson0=nodes[inode].nson;

   for(i=0; i<nson0; i++)
      PruneSubTreeN(nodes[inode].sons[i], keep);

   /* remove inode because of no descendents.  
      Note this does not touch the father node */
   if(inode<com.ns && keep[inode]==0)
      nodes[inode].father=-1;
   else if(inode>=com.ns) {
      for(i=0,nodes[inode].nson=0; i<nson0; i++) {
         ison=nodes[inode].sons[i];
         if(nodes[ison].father!=-1) 
            nodes[inode].sons[ nodes[inode].nson++ ] = nodes[inode].sons[i];
      }
      if(nodes[inode].nson==0)
         nodes[inode].father=-1;
   }

   /* remove inode if it has a single descendent ison */
   if(inode>=com.ns && nodes[inode].nson==1 && inode!=tree.root) {
      ison=nodes[inode].sons[0];
      nodes[ison].father=father;
      nodes[ison].branch+=nodes[inode].branch;
      for(j=0;j<nodes[father].nson;j++) {
         if(nodes[father].sons[j]==inode) 
            { nodes[father].sons[j]=ison; break; }
      }
      nodes[inode].nson=0;
      nodes[inode].father=-1;
   }
   /* move down the root if the root has only one descendent */
   else if(inode==tree.root) {
      if(nodes[inode].nson==1) {
         for(; ; inode=nodes[inode].sons[0]) {
            nodes[inode].father=-1;
            if(nodes[inode].nson>1) break;
            nodes[inode].nson=0;
         }
         tree.root=inode;
         /* collapse down the root. ison is new root */
         if(!com.clock && Nsonroot>=3 && nodes[inode].nson==2)  DeRoot();
      }
   }
   return(0);
}


int GetSubTreeN (int keep[], int space[])
{
/* This removes some tips to generate the subtree.  Branch lengths are 
   preserved by summing them up when some nodes are removed.  
   The algorithm use post-order tree traversal to remove tips and nodes.  It 
   then switches to the branch representation to renumber nodes.
   space[] can be NULL.  If not, it returns newnodeNO[], which holds the 
   new node numbers; for exmaple, newnodeNO[12]=5 means that old node 12 now 
   becomes node 5.

   The routine does not change com.ns or com.spname[], which have to be updated 
   outside.

   CHANGE OF ROOT happens if the root in the old tree had >=3 sons, but has 2 
   sons in the new tree and if (!com.clock).  In that case, the tree is derooted.

   This routine does not work if a current seq is ancestral to some others 
   and if that sequence is removed. (***check this comment ***)
   
   Different formats for keep[] are used.  Suppose the current tree is for 
   nine species: a b c d e f g h i.
   
   (A) keep[]={1,0,1,1,1,0,0,1,0} means that a c d e h are kept in the tree.  
       The old tip numbers are not changed, so that OutaTreeN(?,1,?) gives the 
       correct species names or OutaTreeN(?,0,?) gives the old species numbers.

   (B) keep[]={1,0,2,3,4,0,0,5,0} means that a c d e h are kept in the tree, and 
       they are renumbered 0 1 2 3 4 and all the internal nodes are renumbered 
       as well to be consecutive.  Note that the positive numbers have to be 
       consecutive natural numbers.

       keep[]={5,0,2,1,4,0,0,3,0} means that a c d e h are kept in the tree.  
       However, the order of the sequences are changed to d c h e a, so that the 
       numbers are now 0 1 2 3 4 for d c h e a.  This is useful when the subtree 
       is extracted from a big tree for a subset of the sequence data, while the 
       species are odered d c h e a in the sequence data file.
       This option can be used to renumber the tips in the complete tree.
*/
   int nsnew, i,j,k, nnode0=tree.nnode, sumnumber=0, newnodeNO[2*NS-1];
   double *branch0;
   int debug=0;

   Nsonroot=nodes[tree.root].nson;

   if(debug) { FOR(i,com.ns) printf("%-15s %2d\n", com.spname[i], keep[i]); }
   for(i=0,nsnew=0;i<com.ns;i++)
      if(keep[i]) { nsnew++; sumnumber+=keep[i]; }
   if(nsnew<2)  return(-1);

   /* mark removed nodes in the big tree by father=-1 && nson=0 */
   PruneSubTreeN(tree.root, keep);
   if(debug) printtree(1);

   for(i=0,k=1; i<tree.nnode; i++) if(nodes[i].father!=-1) k++;
   tree.nnode=k;
   NodeToBranch();

   if(sumnumber>nsnew) {
      if(sumnumber!=nsnew*(nsnew+1)/2) error2("keep[] not right in GetSubTreeN");
      if((branch0=(double*)malloc(nnode0*sizeof(double)))==NULL) error2("oom#");
      FOR(i,nnode0) branch0[i]=nodes[i].branch;
      FOR(i,nnode0) newnodeNO[i]=-1;
      FOR(i,com.ns) if(keep[i]) newnodeNO[i]=keep[i]-1;

      newnodeNO[tree.root] = k = nsnew;  tree.root=k++;
      for( ; i<nnode0; i++) {
         if(nodes[i].father==-1) continue;
         for(j=0; j<tree.nbranch; j++) if(i==tree.branches[j][1]) break;
         if(j==tree.nbranch) error2("strange here");
         newnodeNO[i]=k++;
      }
      for(j=0; j<tree.nbranch; j++) FOR(i,2)
         tree.branches[j][i] = newnodeNO[tree.branches[j][i]];
      BranchToNode();
      for(i=0;i<nnode0;i++) {
         if(newnodeNO[i]>-1)
            nodes[newnodeNO[i]].branch=branch0[i];
      }
      free(branch0);
   }
   if(space) memmove(space, newnodeNO, (com.ns*2-1)*sizeof(int));
   return (0);
}


void printtree (int timebranches)
{
   int i,j;

   printf("\nns = %d  nnode = %d", com.ns, tree.nnode);
   printf("\n%7s%7s", "father","node");
   if(timebranches)  printf("%10s%10s%10s", "time","branch","label");
   printf(" %7s%7s", "nson:","sons");
   FOR (i, tree.nnode) {
      printf ("\n%7d%7d", nodes[i].father, i);
      if(timebranches)
         printf(" %9.6f %9.6f %9.0f", nodes[i].age, nodes[i].branch,nodes[i].label);

      printf ("%7d: ", nodes[i].nson);
      FOR(j,nodes[i].nson) printf(" %2d", nodes[i].sons[j]);
   }
   FPN(F0); 
   OutaTreeN(F0,0,0); FPN(F0); 
   OutaTreeN(F0,1,0); FPN(F0); 
   OutaTreeN(F0,1,1); FPN(F0); 
}


void PointconPnodes (void)
{
/* This points the nodes[com.ns+inode].conP to the right space in com.conP.
   The space is different depending on com.cleandata (0 or 1)
   This routine updates internal nodes com.conP only.  
   End nodes (com.conP0) are updated in InitConditionalPNode().
*/
   int nintern=0, i;

   FOR(i,tree.nbranch+1)
      if(nodes[i].nson>0)  /* more thinking */
         nodes[i].conP = com.conP + com.ncode*com.npatt*nintern++;
}


int SetxInitials (int np, double x[], double xb[][2])
{
/* This forces initial values into the boundary of the space
*/
   int i;
   double small=1e-6;

   for (i=com.ntime; i<np; i++) {
      if (x[i]<xb[i][0]*1.1) x[i]=xb[i][0]*1.1;
      if (x[i]>xb[i][1]/1.1) x[i]=xb[i][1]/1.1;
   }
   for (i=0; i<com.np; i++) {
      if (x[i]<xb[i][0]) x[i]=xb[i][0]*1.3;
      if (x[i]>xb[i][1]) x[i]=xb[i][1]*.7;
   }
   return(0);
}


#if(defined(BASEML) || defined(CODEML))

double *AgeLow=NULL;
int NFossils=0, AbsoluteRate=0;
double ScaleTimes_TipDate=1, TipDate=0;
/* TipDate models: 
      MutationRate = mut/ScaleTimes_TipDate; 
      age=age*ScaleTimes_TipDate 
*/

void SetAge(int inode, double x[]);
void GetAgeLow (int inode);
/* number of internal node times, usd to deal with known ancestors.  Broken? */
static int innode_time=0;  

/* Ziheng Yang, 25 January 2003
   The following routines deal with clock and local clock models, including 
   Andrew Rambaut's TipDate models (Rambaut 2000 Bioinformatics 16:395-399;
   Yoder & Yang 2000 Mol Biol Evol 17:1081-1090; Yang & Yoder 2003 Syst Biol).
   The tree is rooted.  The routine SetAge assumes that ancestral nodes are
   arranged in the increasing order and so works only if the input tree uses 
   the parenthesis notation and not the branch notation.  The option of known 
   ancestors is probably broken.

   The flag AbsoluteRate=1 if(TipDate || NFossils).  This could be removed
   as the flags TipDate and NFossils are sufficient.

      clock = 1: global clock, deals with TipDate with no or many fossils, 
                 ignores branch rates (#) in tree if any.
            = 2: local clock models, as above, but requires branch rates # 
                 in tree.
            = 3: as 2, but requires Mgene and option G in sequence file.

   Order of variables in x[]: divergence times, rates for branches, rgene, ...
   In the following ngene=4, com.nbtype=3, with r_ij to be the rate 
   of gene i and branch class j.

   clock=1 or 2:
      [times, r00(if absolute) r01 r02  rgene1 rgene2 rgene3]
      NOTE: rgene[] has relative rates
   clock=3:
      [times, r00(if absolute) r01 r02  r11 r12  r21 r22 r31 r32 rgene1 rgene2 rgene3]
      NOTE: rgene1=r10, rgene2=r20, rgene3=r30

   If(nodes[tree.root].fossil==0) x[0] has absolute time for the root.  
   Otherwise x[0] has proportional ages.
*/


double GetBranchRate(int igene, int ibrate, double x[], int *ix)
{
/* This finds the right branch rate in x[].  The rate is absolute if AbsoluteRate.
   ibrate=0,1,..., indicates the branch rate class.
   This routine is used in the likeihood calculation and in formatting output.
   ix (k) has the position in x[] for the branch rate if the rate is a parameter.
   and is -1 if the rate is not a parameter in the ML iteration.  This is 
   for printing SEs.
*/
   int nage=tree.nnode-com.ns-NFossils, k=nage+AbsoluteRate;
   double rate00=(AbsoluteRate?x[nage]:1), brate=rate00;

   if(igene==0 && ibrate==0)
      k=(AbsoluteRate?nage:-1);
   else if(com.clock==GlobalClock) {
      brate=x[k=com.ntime+igene-1];  /* igene>0, rgene[] has absolute rates */
   }
   else if(com.clock==LocalClock) {  /* rgene[] has relative rates */
      if(igene==0 && ibrate)     { brate=x[k+=ibrate-1]; }
      else if(igene && ibrate==0){ brate=rate00*x[com.ntime+igene-1]; k=-1; }
      else if(igene && ibrate)   { brate=x[k+ibrate-1]*x[com.ntime+igene-1]; k=-1; }
   }
   else if(com.clock==ClockCombined) {
      if(ibrate==0 && igene)  brate=x[k=com.ntime+igene-1];
      else                    brate=x[k+=ibrate-1+igene*(com.nbtype-1)]; /* ibrate>0 */
   }

   if(ix) *ix=k;
   return(brate);
}

int GetTipDate (void)
{
/* This scans sequences for @ to collect dates if (com.clock), for Andrew 
   Rambaut's TipDate models.  This routine is called from GetInitialsTimes()
   for each tree.
   Divergence times are rescaled by using ScaleTimes_TipDate.
*/
   int i, ndates=0, mark='@';
   double young=-1,old=-1;
   char *p;

   TipDate=0;
   ScaleTimes_TipDate=1;
   for(i=0,ndates=0; i<com.ns; i++) {
      nodes[i].age=0;
      p=strchr(com.spname[i], mark);
      if(p==NULL) continue;
      ndates++;
      sscanf(p+1, "%lf", &nodes[i].age);
      if(nodes[i].age<0) error2("tip date<0");
      if(i==0) young=old=nodes[i].age;
      else { old=min2(old,nodes[i].age); young=max2(young,nodes[i].age); }
   }
   if(ndates==0)  return(0);
   
   /* TipDate models */
   if(ndates!=com.ns) 
      error2("TipDate model: each sequence must have a date");
   TipDate=young;
   ScaleTimes_TipDate=(TipDate-old)*5;
   if(ScaleTimes_TipDate==0) error2("All sequences of the same age?");
   for(i=0; i<tree.nnode; i++) {
      if(i<com.ns || nodes[i].fossil)
         nodes[i].age=(TipDate-nodes[i].age)/ScaleTimes_TipDate;
   }

   if(noisy) printf("\nTipDate model: Date range: (%.2f, %.2f), (0, %.2f) after scaling\n",
                     young, old, (young-old)/ScaleTimes_TipDate);

   return(1);
}


void SetAge (int inode, double x[])
{
/* This is called from SetBranch(), to set up age for nodes under clock 
   models (clock=1,2,3).
   if(TipDate||NFossil), that is, if(AbsoluteRate), this routine sets up 
   times (nodes[].age) and then SetBranch() sets up branch lengths by
   multiplying times with rate:
      [].age[i] = AgeLov[i]+([father].age-AgeLov[i])*x[i]
   
   The routine assumes that times are arranged in the order of node numbers, 
   and should work if parenthesis notation of tree is used in the tree file, 
   but not if the branch notation is used.
*/
   int i,ison;

   FOR (i,nodes[inode].nson) {
      ison=nodes[inode].sons[i];
      if(nodes[ison].nson) {
         if(AbsoluteRate) {
            if(!nodes[ison].fossil)
               nodes[ison].age = AgeLow[ison]
                                   +(nodes[inode].age-AgeLow[ison])*x[innode_time++];
         }
         else 
            nodes[ison].age=nodes[inode].age*x[innode_time++];
         SetAge(ison,x);
      }
   }
}

void GetAgeLow (int inode)
{
/* This sets AgeLow[], the minimum age of each node.  It moves down the tree to 
   scan [].age, which has tip dates and fossil dates.  It is needed if(AbsoluteRate)
   and is called by GetInitialsTimes().
*/
   int i,ison;
   double tlow=0;

   FOR(i, nodes[inode].nson) {
      ison=nodes[inode].sons[i];
      if(nodes[ison].nson)
         GetAgeLow(ison);
      tlow = max2(tlow, nodes[ison].age);
   }
   if(nodes[inode].fossil) {
      if(nodes[inode].age<tlow) 
         error2("age in tree is in conflict.");
      AgeLow[inode]=nodes[inode].age;
   }
   else
      AgeLow[inode]=nodes[inode].age=tlow;
}



int SetBranch (double x[])
{
/* if(AbsoluteRate), mutation rate is not multiplied here, but during the 
   likelihood calculation.  It is copied into com.rgene[0].
*/
   int i, status=0;
   double small=-1e-5;

   if(com.clock==0) {
      FOR(i,tree.nnode) {
         if(i!=tree.root) 
            if((nodes[i].branch=x[nodes[i].ibranch])<small)  status=-1;
      }
      return(status);
   }
   innode_time=0;
   if(!LASTROUND) { /* transformed variables (proportions) are used */
      if(!nodes[tree.root].fossil) /* note order of times in x[] */
         nodes[tree.root].age=x[innode_time++];
      SetAge(tree.root, x);
   }
   else {           /* times are used */
      for(i=com.ns; i<tree.nnode; i++) 
         if(!nodes[i].fossil) nodes[i].age=x[innode_time++];
   }

   FOR (i,tree.nnode) {  /* [].age to [].branch */
      if(i==tree.root) continue;
      nodes[i].branch = nodes[nodes[i].father].age-nodes[i].age;
      if(nodes[i].branch<small)
         status=-1;
   }
   return(status);
}


int GetInitialsTimes (double x[])
{
/* this counts com.ntime and initializes x[] under clock and local clock models,
   including TipDate and ClockCombined models.  See above for notes.
   Under local clock models, com.ntime includes both times and rates for 
   lineages.
   A recursive algorithm is used to specify initials if(TipDate||NFossil).
*/
   int i,j,k;
   double maxage,t;

   /* no clock */
   if(com.fix_blength==2)  
      { com.ntime=0; com.method=0; return(0); }
   else if(com.clock==0) { 
      com.ntime=tree.nbranch;
      if(com.fix_blength==-1)
         FOR (i,com.ntime) x[i]=rndu();
      else if(com.fix_blength==0) {
         FOR (i,com.ntime) x[i]=rndu();
         if(com.clock<5 && ancestor && com.ntime<200)
            LSDistance (&t, x, testx);
      }
      return(0);
   }
 
   /* clock models: check branch rate labels and fossil dates first */
   if(com.clock<5) {
      com.nbtype=1;
      if(com.clock==1) 
         for(i=0; i<tree.nnode; i++) nodes[i].label=0;
      else {
         for(i=0; i<tree.nnode; i++) {
            if(i!=tree.root && (j=(int)nodes[i].label+1)>com.nbtype) {
               com.nbtype=j;
               if(j<0||j>tree.nbranch-1) error2("branch label in the tree.");
            }
         }
         if(noisy) printf("\nfound %d branch rates in tree.\n", com.nbtype);
         if(com.nbtype<=1) error2("use clock = 1 or add branch rate labels in tree");

         FOR(i,tree.nbranch) printf("%3.0f",nodes[tree.branches[i][1]].label); FPN(F0);
      }
   }
   for(i=0,NFossils=0,maxage=0; i<tree.nnode; i++) {
      if(nodes[i].nson && nodes[i].fossil) {
         NFossils ++;
         maxage=max2(maxage,nodes[i].age);
      }
   }
   if(NFossils && maxage>10) 
      error2("Change time unit so that fossil dates fall in (0.00001, 10).");

   GetTipDate();
   AbsoluteRate=(TipDate || NFossils);
   if(com.clock>=5 && AbsoluteRate==0) 
      error2("needs fossil calibrations");

   com.ntime=AbsoluteRate+(tree.nnode-com.ns-NFossils)+(com.nbtype-1);
   if(com.clock==ClockCombined)  com.ntime += (com.ngene-1)*(com.nbtype-1);
   com.ntime += (tree.root<com.ns); /* root is a known sequence. Broken? */

   /* DANGER! AgeLow is not freed in the program. Fix this? */
   k=0;
   if(AbsoluteRate) {
      AgeLow=(double*)realloc(AgeLow, tree.nnode*sizeof(double));
      GetAgeLow(tree.root);
   }
   if(!nodes[tree.root].fossil)
      x[k++] = (AbsoluteRate?nodes[tree.root].age*(1.2+rndu()) : rndu()+.5);  /* root age */
   for(; k<tree.nnode-com.ns-NFossils; k++)   /* relative times */
      x[k]=0.4+.5*rndu();
   if(com.clock!=6)                           /* branch rates */
      for( ; k<com.ntime; k++)
         x[k]=0.1*(.5+rndu());
   else
      for(j=0,k=com.ntime-1; j<data.ngene; j++,k++) 
         x[k]=0.1*(.5+rndu());
   return(0);
}

int OutputTimesRates (FILE *fout, double x[], double var[])
{
/* SetBranch() has been called before calling this, so that [].age is up 
   to date.
*/
   int i,j,k=AbsoluteRate+tree.nnode-com.ns-NFossils, jeffnode;
   double scale=(TipDate?ScaleTimes_TipDate:1);

   /* rates */
   if(AbsoluteRate && com.clock<5) {
      fputs("\nSubstitution rate is per time unit\n", fout);
      if(com.nbtype>1) fprintf(fout,"Rates for branch groups\n");
      for(i=0; i<com.ngene; i++,FPN(fout)) {
         if(com.ngene>1) fprintf(fout,"Gene %2d: ", i+1);
         for(j=0; j<com.nbtype; j++) {
            fprintf(fout,"%12.6f", GetBranchRate(i,j,x,&k)/scale);
            if(i==0 && j==0 && !AbsoluteRate) continue;
            if((com.clock!=LocalClock||com.ngene==1) && com.getSE) {
               if(k==-1) error2("we are in trouble. k should not be -1 here.");
               fprintf(fout," +- %8.6f", sqrt(var[k*com.np+k])/scale);
            }
         }
      }
   }
   else 
      if(com.clock==2) {
         fprintf (fout,"rates for branches:    1");
         for(k=tree.nnode-com.ns; k<com.ntime; k++) fprintf(fout," %8.5f",x[k]);
      }


   /* times */
   if(AbsoluteRate) {
      fputs("\nNodes and Times\n",fout);
      fputs("(JeffNode is for Thorne's multidivtime.  ML analysis uses ingroup data only.)\n\n",fout);
   }
   if(TipDate) { /* DANGER! SE not printed if(TipDate && NFossil). */
      for(i=0,k=0; i<tree.nnode; i++,FPN(fout)) {
         jeffnode=(i<com.ns?i:tree.nnode-1+com.ns-i);
         fprintf(fout,"Node %3d (Jeffnode %3d) Time %7.2f ",i+1, jeffnode, 
            TipDate-nodes[i].age*scale);
         if(com.getSE && i>=com.ns && !nodes[i].fossil) {
            fprintf(fout," +- %6.2f", sqrt(var[k*com.np+k])*scale);
            k++;
         }
      }
   }
   else if(AbsoluteRate) {
      for(i=com.ns,k=0; i<tree.nnode; i++,FPN(fout)) {
         jeffnode=tree.nnode-1+com.ns-i;
         fprintf(fout,"Node %3d (Jeffnode %3d) Time %9.5f", i+1, tree.nnode-1+com.ns-i, 
            nodes[i].age);
         if(com.getSE && i>=com.ns && !nodes[i].fossil) {
            fprintf(fout," +- %7.5f", sqrt(var[k*com.np+k]));
            if(fabs(nodes[i].age-x[k])>1e-5) error2("node order wrong.");
            k++;
         }
      }
   }

   return(0);
}

int SetxBoundTimes (double xb[][2])
{
/* This sets bounds for times (or branch lengths) and branch rates
*/ 
   int i=-1,j,k;
   double tb[]={4e-6,50}, tb0=1e-8, rateb[]={1e-4,99}, pb[]={.000001,.999999};

   if(com.clock==0) {
      for(i=0;i<com.ntime;i++) {
         xb[i][1] = tb[1];
         /* internal vs external */
         xb[i][0] = (tree.branches[i][1]<com.ns ? tb[0] : tb0); 
      }
   }
   else {
      k=0;  xb[0][0]=tb[0];  xb[0][1]=tb[1];
      if(!nodes[tree.root].fossil) {
         if(AbsoluteRate)  xb[0][0]=AgeLow[tree.root];
         k=1;
      }
      for( ; k<tree.nnode-com.ns-NFossils; k++)  /* proportional ages */
         { xb[k][0]=pb[0]; xb[k][1]=pb[1]; }
      for(; k<com.ntime; k++)                    /* rate and branch rates */
         FOR(j,2) xb[k][j]=rateb[j];
   }
   return(0);
}

#endif



#if(defined(BASEML) || defined(CODEML))

int CollapsNode (int inode, double x[]) 
{
/* Merge inode to its father. Update the first com.ntime elments of
   x[] only if (x!=NULL), by using either x[] if clock=1 or
   nodes[].branch if clock=0.  So when clock=0, the routine works
   properly only if SetBranch() is called before this routine, which
   is true if m.l. or l.s. has been used to estimate branch lengths.
*/
   int i,j, ifather, ibranch, ison;

   if (inode==tree.root || inode<com.ns) error2("err CollapsNode");
   ibranch=nodes[inode].ibranch;   ifather=nodes[inode].father; 
   for (i=0; i<nodes[inode].nson; i++) {
      ison=nodes[inode].sons[i];
      tree.branches[nodes[ison].ibranch][0]=ifather;
   }
   for (i=ibranch+1; i<tree.nbranch; i++) 
      for (j=0; j<2; j++) tree.branches[i-1][j]=tree.branches[i][j];
   tree.nbranch--; com.ntime--;
   for (i=0; i<tree.nbranch; i++)  for (j=0; j<2; j++) 
        if (tree.branches[i][j]>inode)  tree.branches[i][j]--;
   BranchToNode();

   if (x) {
      if (com.clock) 
         for (i=inode+1; i<tree.nnode+1; i++) x[i-1-com.ns]=x[i-com.ns];
      else {
         for (i=ibranch+1; i<tree.nbranch+1; i++)  x[i-1]=x[i];
         SetBranch (x);
      }
   }
   return (0);
}

#endif



void DescentGroup (int inode);
void BranchPartition (char partition[], int parti2B[]);

static char *PARTITION;

void DescentGroup (int inode)
{
   int i;
   for (i=0; i<nodes[inode].nson; i++) 
      if (nodes[inode].sons[i]<com.ns) 
         PARTITION[nodes[inode].sons[i]]=1;
      else 
         DescentGroup (nodes[inode].sons[i]);
}

void BranchPartition (char partition[], int parti2B[])
{
/* calculates branch partitions.
   partition[0,...,ns-1] marks the species bi-partition by the first interior
   branch.  It uses 0 and 1 to indicate which side of the branch each species
   is.
   partition[ns,...,2*ns-1] marks the second interior branch.
   parti2B[0] maps the partition (internal branch) to the branch in tree.
   Use NULL for parti2B if this information is not needed.
   partition[nib*com.ns].  nib: # of interior branches.
*/
   int i,j, nib;  /* number of internal branches */

   for (i=0,nib=0; i<tree.nbranch; i++) {
      if (tree.branches[i][1]>=com.ns){
         PARTITION=partition+nib*com.ns;
         FOR (j,com.ns) PARTITION[j]=0;
         DescentGroup (tree.branches[i][1]);
         if (parti2B) parti2B[nib]=i;
         nib++;
      }
   }
   if (nib!=tree.nbranch-com.ns) error2("err BranchPartition"); 
}


int NSameBranch (char partition1[],char partition2[], int nib1,int nib2,
    int IBsame[])
{
/* counts the number of correct (identical) bipartitions.
   nib1 and nib2 are the numbers of interior branches in the two trees
   correctIB[0,...,(correctbranch-1)] lists the correct interior branches, 
   that is, interior branches in tree 1 that is also in tree 2.
   IBsame[i]=1 if interior branch i is correct.
*/
   int i,j,k, nsamebranch,nsamespecies;

   for (i=0,nsamebranch=0; i<nib1; i++)  for(j=0,IBsame[i]=0; j<nib2; j++) {
      for (k=0,nsamespecies=0;k<com.ns;k++)
         nsamespecies+=(partition1[i*com.ns+k]==partition2[j*com.ns+k]);
      if (nsamespecies==0 || nsamespecies==com.ns)
         { nsamebranch++;  IBsame[i]=1;  break; } 
   }
   return (nsamebranch);
}



int AddSpecies (int is, int ib)
{
/* Add species (is) to tree at branch ib.  The tree currently has 
   is+1-1 species.  Interior node numbers are increased by 2 to make 
   room for the new nodes.
   if(com.clock && ib==tree.nbranch), the new species is added as an
   outgroup to the rooted tree.
*/
   int i,j, it;

   if(ib>tree.nbranch+1 || (ib==tree.nbranch && !com.clock)) return(-1);

   if(ib==tree.nbranch && com.clock) { 
      FOR(i,tree.nbranch) FOR(j,2)
         if (tree.branches[i][j]>=is) tree.branches[i][j]+=2;
      it=tree.root;  if(tree.root>=is) it+=2;
      FOR(i,2) tree.branches[tree.nbranch+i][0]=tree.root=is+1;
      tree.branches[tree.nbranch++][1]=it;
      tree.branches[tree.nbranch++][1]=is;
   }
   else {
      FOR(i,tree.nbranch) FOR(j,2)
         if (tree.branches[i][j]>=is) tree.branches[i][j]+=2;
      it=tree.branches[ib][1];
      tree.branches[ib][1]=is+1;
      tree.branches[tree.nbranch][0]=is+1;
      tree.branches[tree.nbranch++][1]=it;
      tree.branches[tree.nbranch][0]=is+1;
      tree.branches[tree.nbranch++][1]=is;
      if (tree.root>=is) tree.root+=2;
   }
   BranchToNode ();
   return (0);
}


#ifdef TREESEARCH

static struct TREE
  {struct TREEB tree; struct TREEN nodes[2*NS-1]; double x[NP]; } 
  treebest, treestar;
/*
static struct TREE 
  {struct TREEB tree; struct TREEN nodes[2*NS-1];} treestar;
*/

int Perturbation(FILE* fout, int initialMP, double space[]);

int Perturbation(FILE* fout, int initialMP, double space[])
{
/* heuristic tree search by the NNI tree perturbation algorithm.  
   Some trees are evaluated multiple times as no trees are kept.
   This needs more work.
*/
   int step=0, ntree=0, nmove=0, improve=0, ineighb, i,j;
   int sizetree=(2*com.ns-1)*sizeof(struct TREEN);
   double *x=treestar.x;
   FILE *ftree;

if(com.clock) error2("\n\aerr: pertubation does not work with a clock yet.\n");
if(initialMP&&!com.cleandata)
   error2("\ncannot get initial parsimony tree for gapped data yet.");

   fprintf(fout, "\n\nHeuristic tree search by NNI perturbation\n");
   if (initialMP) {
      if (noisy) printf("\nInitial tree from stepwise addition with MP:\n");
      fprintf(fout, "\nInitial tree from stepwise addition with MP:\n");
      StepwiseAdditionMP (space);
   }
   else {
      if (noisy) printf ("\nInitial tree read from file %s:\n", com.treef);
      fprintf(fout, "\nInitial tree read from file.\n");
      if ((ftree=fopen (com.treef,"r"))==NULL) error2("treefile not exist?");
      fscanf (ftree, "%d%d", &i, &ntree);
      if (i!=com.ns) error2("ns in the tree file");
      if(ReadaTreeN (ftree, &i, &j, 0, 1)) error2("err tree..");
      fclose(ftree);
   }
   if (noisy) { FPN (F0);  OutaTreeN(F0,0,0);  FPN(F0); }
   tree.lnL=TreeScore(x, space);
   if (noisy) { OutaTreeN(F0,0,1);  printf("\n lnL = %.4f\n",-tree.lnL); }
   OutaTreeN(fout,1,1);  fprintf(fout, "\n lnL = %.4f\n",-tree.lnL);
   if (com.np>com.ntime) {
      fprintf(fout, "\tparameters:"); 
      for(i=com.ntime; i<com.np; i++) fprintf(fout, "%9.5f", x[i]);
      FPN(fout);
   }
   fflush(fout);
   treebest.tree=tree;  memcpy(treebest.nodes, nodes, sizetree);

   for (step=0; ; step++) {
      for (ineighb=0,improve=0; ineighb<(tree.nbranch-com.ns)*2; ineighb++) {
         tree=treebest.tree; memcpy (nodes, treebest.nodes, sizetree);
         NeighborNNI (ineighb);
         if(noisy) {
            printf("\nTrying tree # %d (%d move[s]) \n", ++ntree,nmove);
            OutaTreeN(F0,0,0);  FPN(F0);
         }
         tree.lnL=TreeScore(x, space);
         if (noisy) { OutaTreeN(F0,1,1); printf("\n lnL = %.4f\n",-tree.lnL);}
         if (noisy && com.np>com.ntime) {
            printf("\tparameters:"); 
            for(i=com.ntime; i<com.np; i++) printf("%9.5f", x[i]);
            FPN(F0);
         }
         if (tree.lnL<=treebest.tree.lnL) {
            treebest.tree=tree;  memcpy (treebest.nodes, nodes, sizetree);
            improve=1; nmove++;
            if (noisy) printf(" moving to this tree\n");
            if (fout) {
               fprintf(fout, "\nA better tree:\n");
               OutaTreeN(fout,0,0); FPN(fout); OutaTreeN(fout,1,1); FPN(fout); 
               fprintf(fout, "\nlnL = %.4f\n", tree.lnL);
               if (com.np>com.ntime) {
                  fprintf(fout,"\tparameters:"); 
                  for(i=com.ntime; i<com.np; i++) fprintf(fout,"%9.5f", x[i]);
                  FPN(fout);
               }
               fflush(fout);
          }
         }
      }
      if (!improve) break;
   }
   tree=treebest.tree;  memcpy (nodes, treebest.nodes, sizetree);
   if (noisy) {
      printf("\n\nBest tree found:\n");
      OutaTreeN(F0,0,0);  FPN(F0);  OutaTreeN(F0,1,1);  FPN(F0); 
      printf("\nlnL = %.4f\n", tree.lnL);
   }
   if (fout) {
      fprintf(fout, "\n\nBest tree found:\n");
      OutaTreeN(fout,0,0);  FPN(fout);  OutaTreeN(fout,1,1);  FPN(fout); 
      fprintf(fout, "\nlnL = %.4f\n", tree.lnL);
   }
   return (0);
}


static int *_U0, *_step0, _mnnode;
/* up pass characters and changes for the star tree: each of size npatt*nnode*/

int StepwiseAdditionMP (double space[])
{
/* tree search by species addition.
*/
   char *z0[NS];
   int  ns0=com.ns, is, i,j,h, tiestep=0,tie,bestbranch=0;
   int sizetree=(2*com.ns-1)*sizeof(struct TREEN);
   double bestscore=0,score;

   _mnnode=com.ns*2-1;
   _U0=(int*)malloc(com.npatt*_mnnode*sizeof(int));
   _step0=(int*)malloc(com.npatt*_mnnode*sizeof(int));
   if (noisy>2) 
     printf("\n%9ld bytes for MP (U0 & N0)\n", 2*com.npatt*_mnnode*sizeof(int));
   if (_U0==NULL || _step0==NULL) error2("oom U0&step0");

   FOR (i,ns0)  z0[i]=com.z[i];
   tree.nbranch=tree.root=com.ns=3;
   FOR (i, tree.nbranch) { tree.branches[i][0]=com.ns; tree.branches[i][1]=i; }
   BranchToNode ();
   FOR (h, com.npatt)
      FOR (i,com.ns)
        { _U0[h*_mnnode+i]=1<<(com.z[i][h]-1); _step0[h*_mnnode+i]=0; }
   for (is=com.ns,tie=0; is<ns0; is++) {
      treestar.tree=tree;  memcpy (treestar.nodes, nodes, sizetree);

      for (j=0; j<treestar.tree.nbranch; j++,com.ns--) {
         tree=treestar.tree;  memcpy (nodes, treestar.nodes, sizetree);
         com.ns++;
         AddSpecies (is, j);
         score=MPScoreStepwiseAddition(is, space, 0);
/*
OutaTreeN(F0, 0, 0); 
printf(" Add sp %d (ns=%d) at branch %d, score %.0f\n", is+1,com.ns,j+1,score);
*/
         if (j && score==bestscore) tiestep=1;
         if (j==0 || score<bestscore || (score==bestscore&&rndu()<.1)) {
            tiestep=0;
            bestscore=score; bestbranch=j;
         }
      }
      tie+=tiestep;
      tree=treestar.tree;  memcpy (nodes, treestar.nodes, sizetree);
      com.ns=is+1;
      AddSpecies (is, bestbranch);
      score=MPScoreStepwiseAddition(is, space, 1);

      if (noisy)
       { printf("\r  Added %d [%5.0f steps]",is+1,-bestscore); fflush(F0);}
   }
   if (noisy>2) printf("  %d stages with ties, ", tie);
   tree.lnL=bestscore;
   free(_U0); free(_step0);
   return (0);
}

double MPScoreStepwiseAddition (int is, double space[], int save)
{
/* this changes only the part of the tree affected by the newly added 
   species is.
   save=1 for the best tree, so that _U0 & _step0 are updated
*/
   int *U,*N,U3[3], h,ist, i,father,son2,*pU0=_U0,*pN0=_step0;
   double score;

   U=(int*)space;  N=U+_mnnode;
   for (h=0,score=0; h<com.npatt; h++,pU0+=_mnnode,pN0+=_mnnode) {
      FOR (i, tree.nnode) { U[i]=pU0[i-2*(i>=is)]; N[i]=pN0[i-2*(i>=is)]; }
      U[is]=1<<(com.z[is][h]-1);  N[is]=0;
      for (ist=is; (father=nodes[ist].father)!=tree.root; ist=father) {
         if ((son2=nodes[father].sons[0])==ist)  son2=nodes[father].sons[1];
         N[father]=N[ist]+N[son2];
         if ((U[father]=U[ist]&U[son2])==0)
            { U[father]=U[ist]|U[son2];  N[father]++; }
      }
      FOR (i,3) U3[i]=U[nodes[tree.root].sons[i]];
      N[tree.root]=2;
      if (U3[0]&U3[1]&U3[2]) N[tree.root]=0;
      else if (U3[0]&U3[1] || U3[1]&U3[2] || U3[0]&U3[2]) N[tree.root]=1;
      FOR(i,3) N[tree.root]+=N[nodes[tree.root].sons[i]];

      if (save) {
         memcpy (pU0, U, tree.nnode*sizeof(int));
         memcpy (pN0, N, tree.nnode*sizeof(int));
      }
      score+=N[tree.root]*com.fpatt[h];
   }
   return (score);
}


int readx(double x[], int *fromfile)
{
/* this reads parameters from file, used as initial values
   if(runmode>0), this reads common substitution parameters only into x[], which 
   should be copied into another place before heuristic tree search.  This is broken
   right now.  Ziheng, 9 July 2003.
   fromfile=0: if nothing read from file, 1: read from file, -1:fix parameters
*/
   static int times=0;
   int i, npin;
   double *xin;

   times++;  *fromfile=0;
   if(finitials==NULL || (com.runmode>0 && times>1)) return(0);
   if(com.runmode<=0) { npin=com.np; xin=x; }
   else               { npin=com.np-com.ntime; xin=x+com.ntime; }

   if(npin<=0) return(0);
   if(com.runmode>0&&com.seqtype==1&&com.model) error2("option or in.codeml");
   printf("\nReading initials/paras from file (np=%d). Stop if wrong.\n",npin);
   fscanf(finitials,"%lf",&xin[i=0]);
   *fromfile=1;
   if(xin[0]==-1) { *fromfile=-1; LASTROUND=1; }
   else           i++;
   for( ; i<npin; i++) if(fscanf(finitials,"%lf",&xin[i])!=1) break;
   if(i<npin)
      { printf("err at #%d. Edit or remove it.\n",i+1); exit(-1); }
   if(com.runmode>0) {
      matout(F0,xin,1,npin);
      puts("Those are fixed for tree search.  Stop if wrong.");
   }
   return(0);
}


double TreeScore(double x[], double space[])
{
   static int fromfile=0;
   int i;
   double xb[NP][2], e=1e-6, lnL=0;

   if(com.clock==2) error2("local clock in TreeScore");
   com.ntime = com.clock ? tree.nnode-com.ns : tree.nbranch;

   GetInitials(x, &i);  /* this shoulbe be improved??? */
   if(i) fromfile=1;
   PointconPnodes();

   if(com.method==0 || !fromfile) SetxBound(com.np, xb);
#ifndef SIMULATE
   if(!com.cleandata) InitConditionalPNode ();
#endif

   if(fromfile) {
      lnL=com.plfun(x,com.np);
      com.np=com.ntime;
   }
   NFunCall=0;
   if(com.method==0 || com.ntime==0)
      ming2(NULL,&lnL,com.plfun,NULL,x,xb, space,e,com.np);
   else
      minB(NULL, &lnL, x, xb, e, space);

   return(lnL);
}


int StepwiseAddition (FILE* fout, double space[])
{
/* heuristic tree search by species addition.  Species are added in the order 
   of occurrence in the data.
   Try to get good initial values.
*/
   char *z0[NS], *spname0[NS];
   int ns0=com.ns, is, i,j, bestbranch=0, randadd=0, order[NS];
   int sizetree=(2*com.ns-1)*sizeof(struct TREEN);
   double bestscore=0,score, *x=treestar.x;

   if(com.ns<3) error2("2 sequences, no need for tree search");
   if (noisy) printf("\n\nHeuristic tree search by stepwise addition\n");
   if (fout) fprintf(fout, "\n\nHeuristic tree search by stepwise addition\n");
   FOR (i,ns0)  { z0[i]=com.z[i]; spname0[i]=com.spname[i]; }
   tree.nbranch=tree.root=com.ns=(com.clock?2:3);  

   FOR(i,ns0) order[i]=i;
   if(randadd) {
      FOR(i,ns0)
         { j=(int)(ns0*rndu()); is=order[i]; order[i]=order[j]; order[j]=is; }
      if(noisy) FOR(i,ns0) printf(" %d", order[i]+1);
      if(fout) { 
         fputs("\nOrder of species addition:\n",fout); 
         FOR(i,ns0)fprintf(fout,"%3d  %-s\n", order[i]+1,com.spname[order[i]]);
      }
      FOR(i,ns0) { com.z[i]=z0[order[i]]; com.spname[i]=spname0[order[i]]; }
   }

   FOR (i,tree.nbranch) { tree.branches[i][0]=com.ns; tree.branches[i][1]=i; }
   BranchToNode ();
   for (is=com.ns; is<ns0; is++) {                  /* add the is_th species */
      treestar.tree=tree;  memcpy (treestar.nodes, nodes, sizetree);

      for (j=0; j<treestar.tree.nbranch+(com.clock>0); j++,com.ns--) { 
         tree=treestar.tree;  memcpy(nodes, treestar.nodes, sizetree);
         com.ns++;
         AddSpecies(is,j);
         score=TreeScore(x, space);
         if (noisy>1)
            { printf("\n "); OutaTreeN(F0, 0, 0); printf("%12.3f",-score); }

         if (j==0 || score<bestscore || (score==bestscore&&rndu()<.2)) {
            treebest.tree=tree;  memcpy(treebest.nodes, nodes, sizetree);
            xtoy (x, treebest.x, com.np);
            bestscore=score; bestbranch=j;
         }
      }
      tree=treebest.tree;  memcpy(nodes,treebest.nodes, sizetree);
      xtoy (treebest.x, x, com.np);
      com.ns=is+1;

      if (noisy) {
         printf("\n\nAdded sp. %d, %s [%.3f]\n",is+1,com.spname[is],-bestscore);
         OutaTreeN(F0,0,0);  FPN(F0);  OutaTreeN(F0,1,0);  FPN(F0);
         if (com.np>com.ntime) {
            printf("\tparameters:"); 
            for(i=com.ntime; i<com.np; i++) printf("%9.5f", x[i]);
            FPN(F0);
         }
      }
      if (fout) {
         fprintf(fout,"\n\nAdded sp. %d, %s [%.3f]\n",
                 is+1, com.spname[is], -bestscore);
         OutaTreeN(fout,0,0); FPN(fout);
         OutaTreeN(fout,1,1); FPN(fout);
         if (com.np>com.ntime) {
            fprintf(fout, "\tparameters:"); 
            for(i=com.ntime; i<com.np; i++) fprintf(fout, "%9.5f", x[i]);
            FPN(fout);
         }
         fflush(fout);
      }
   }  
   tree.lnL=bestscore;

fprintf(frst1, "%12.2f ",bestscore); OutaTreeN(frst1,0,0);

   return (0);
}


int DecompTree (int inode, int ison1, int ison2);
#define hdID(i,j) (max2(i,j)*(max2(i,j)-1)/2+min2(i,j))

int StarDecomposition (FILE *fout, double space[])
{
/* automatic tree search by star decomposition, nhomo<=1
   returns (0,1,2,3) for the 4s problem.
*/
   int status=0,stage=0, i,j, itree,ntree=0,ntreet,best=0,improve=1,collaps=0;
   int inode, nson=0, ison1,ison2, son1, son2;
   int sizetree=(2*com.ns-1)*sizeof(struct TREEN);
   double x[NP];
   FILE *ftree, *fsum=frst;

   if (com.runmode==1) {   /* read the star-like tree from tree file */
      if ((ftree=fopen (com.treef,"r"))==NULL) error2("no treefile");
      fscanf (ftree, "%d%d", &i, &ntree);
      if (ReadaTreeN(ftree, &i, &j, 0, 1)) error2("err tree file");
      fclose (ftree);
   }
   else {                  /* construct the star tree of ns species */
      tree.nnode = (tree.nbranch=tree.root=com.ns)+1;
      for (i=0; i<tree.nbranch; i++)
         { tree.branches[i][0]=com.ns; tree.branches[i][1]=i; }
      com.ntime = com.clock?1:tree.nbranch;
      BranchToNode ();
   }
   if (noisy) { printf("\n\nstage 0: ");  OutaTreeN(F0,0,0); }
   if (fsum) { fprintf(fsum,"\n\nstage 0: ");  OutaTreeN(fsum,0,0); }
   if (fout) { fprintf(fout,"\n\nstage 0: ");  OutaTreeN(fout,0,0); }

   tree.lnL=TreeScore(x,space);

   if (noisy)  printf("\nlnL:%14.6f%6d", -tree.lnL, NFunCall);
   if (fsum) fprintf(fsum,"\nlnL:%14.6f%6d", -tree.lnL, NFunCall);
   if (fout) {
      fprintf(fout,"\nlnL(ntime:%3d  np:%3d):%14.6f\n",
         com.ntime, com.np, -tree.lnL);
      OutaTreeB (fout);  FPN(fout);
      FOR (i, com.np) fprintf (fout,"%9.5f", x[i]);  FPN (fout);
   }
   treebest.tree=tree;  memcpy(treebest.nodes,nodes,sizetree);
   FOR (i,com.np) treebest.x[i]=x[i];
   for (ntree=0,stage=1; ; stage++) {
      for (inode=treebest.tree.nnode-1; inode>=0; inode--) {
         nson=treebest.nodes[inode].nson;
         if (nson>3) break;
         if (com.clock) { if (nson>2) break; }
         else if (nson>2+(inode==treebest.tree.root)) break;
      }
      if (inode==-1 || /*stage>com.ns-3+com.clock ||*/ !improve) { /* end */
         tree=treebest.tree;  memcpy (nodes, treebest.nodes, sizetree);

         if (noisy) {
            printf("\n\nbest tree: ");  OutaTreeN(F0,0,0);
            printf("   lnL:%14.6f\n", -tree.lnL);
         }
         if (fsum) {
            fprintf(fsum, "\n\nbest tree: ");  OutaTreeN(fsum,0,0);
            fprintf(fsum, "   lnL:%14.6f\n", -tree.lnL);
         }
         if (fout) {
            fprintf(fout, "\n\nbest tree: ");  OutaTreeN(fout,0,0);
            fprintf(fout, "   lnL:%14.6f\n", -tree.lnL);
            OutaTreeN(fout,1,1);  FPN(fout);
         }
         break;
      }
      treestar=treebest;  memcpy(nodes,treestar.nodes,sizetree);

      if (collaps && stage) { 
         printf ("\ncollapsing nodes\n");
         OutaTreeN(F0, 1, 1);  FPN(F0);

         tree=treestar.tree;  memcpy(nodes, treestar.nodes, sizetree);
         for (i=com.ns,j=0; i<tree.nnode; i++)
            if (i!=tree.root && nodes[i].branch<1e-7) 
               { CollapsNode (i, treestar.x);  j++; }
         treestar.tree=tree;  memcpy(treestar.nodes, nodes, sizetree);

         if (j)  { 
            fprintf (fout, "\n%d node(s) collapsed\n", j);
            OutaTreeN(fout, 1, 1);  FPN(fout);
         }
         if (noisy) {
            printf ("\n%d node(s) collapsed\n", j);
            OutaTreeN(F0, 1, 1);  FPN(F0);
/*            if (j) getchar (); */
         }
      }

      ntreet = nson*(nson-1)/2;
      if (!com.clock && inode==treestar.tree.root && nson==4)  ntreet=3;
      com.ntime++;  com.np++;

      if (noisy) {
         printf ("\n\nstage %d:%6d trees, ntime:%3d  np:%3d\nstar tree: ",
            stage, ntreet, com.ntime, com.np);
         OutaTreeN(F0, 0, 0);
         printf ("  lnL:%10.3f\n", -treestar.tree.lnL);
      }
      if (fsum) {
       fprintf (fsum, "\n\nstage %d:%6d trees, ntime:%3d  np:%3d\nstar tree: ",
         stage, ntreet, com.ntime, com.np);
         OutaTreeN(fsum, 0, 0);
         fprintf (fsum, "  lnL:%10.6f\n", -treestar.tree.lnL);
      }
      if (fout) {
         fprintf (fout,"\n\nstage %d:%6d trees\nstar tree: ", stage, ntreet);
         OutaTreeN(fout, 0, 0);
         fprintf (fout, " lnL:%14.6f\n", -treestar.tree.lnL);
         OutaTreeN(fout, 1, 1);  FPN (fout);
      }

      for (ison1=0,itree=improve=0; ison1<nson; ison1++)
      for (ison2=ison1+1; ison2<nson&&itree<ntreet; ison2++,itree++,ntree++) {
         DecompTree (inode, ison1, ison2);
         son1=nodes[tree.nnode-1].sons[0];
         son2=nodes[tree.nnode-1].sons[1];

         for(i=com.np-1; i>0; i--)  x[i]=treestar.x[i-1];
         if (!com.clock)
            for (i=0; i<tree.nbranch; i++)
               x[i]=max2(nodes[tree.branches[i][1]].branch*0.99, 0.0001);
         else
            for (i=1,x[0]=max2(x[0],.01); i<com.ntime; i++)  x[i]=.5;

         if (noisy) {
            printf("\nS=%d:%3d/%d  T=%4d  ", stage,itree+1,ntreet,ntree+1);
            OutaTreeN(F0, 0, 0);
         }
         if (fsum) {
         fprintf(fsum, "\nS=%d:%3d/%d  T=%4d  ", stage,itree+1,ntreet,ntree+1);
            OutaTreeN(fsum, 0, 0);
         }
         if (fout) {
           fprintf(fout,"\nS=%d:%4d/%4d  T=%4d ",stage,itree+1,ntreet,ntree+1);
           OutaTreeN(fout, 0, 0);
         }
         tree.lnL=TreeScore(x, space);

         if (tree.lnL<treebest.tree.lnL) {
            treebest.tree=tree;  memcpy (treebest.nodes, nodes, sizetree);
            FOR(i,com.np) treebest.x[i]=x[i];
            best=itree+1;   improve=1;
         }
         if (noisy) printf("%6d%2c %+8.2f", 
                       NFunCall,(status?'?':'X'),treestar.tree.lnL-tree.lnL);
         if (fsum) {
            fprintf(fsum, "%6d%2c", NFunCall, (status?'?':'X'));
            for (i=com.ntime; i<com.np; i++)  fprintf(fsum, "%7.3f", x[i]);
            fprintf(fsum, " %+8.2f", treestar.tree.lnL-tree.lnL);
            fflush(fsum);
         }
         if (fout) {
            fprintf(fout,"\nlnL(ntime:%3d  np:%3d):%14.6f\n",
                         com.ntime, com.np, -tree.lnL);
            OutaTreeB (fout);   FPN(fout);
            FOR (i,com.np) fprintf(fout,"%9.5f", x[i]); 
            FPN(fout); fflush(fout);
         }
      }  /* for (itree) */
      son1=treebest.nodes[tree.nnode-1].sons[0];
      son2=treebest.nodes[tree.nnode-1].sons[1];
   }    /* for (stage) */

   if (com.ns<=4) return (best);
   else return (0);
}

int DecompTree (int inode, int ison1, int ison2)
{
/* decompose treestar at NODE inode into tree and nodes[]
*/
   int i, son1, son2;
   int sizetree=(2*com.ns-1)*sizeof(struct TREEN);
   double bt, fmid=0.001, fclock=0.0001;

   tree=treestar.tree;  memcpy (nodes, treestar.nodes, sizetree);
   for (i=0,bt=0; i<tree.nnode; i++)
      if (i!=tree.root) bt+=nodes[i].branch/tree.nbranch;

   nodes[tree.nnode].nson=2;
   nodes[tree.nnode].sons[0]=son1=nodes[inode].sons[ison1];
   nodes[tree.nnode].sons[1]=son2=nodes[inode].sons[ison2];
   nodes[tree.nnode].father=inode;
   nodes[son1].father=nodes[son2].father=tree.nnode;

   nodes[inode].sons[ison1]=tree.nnode;
   for (i=ison2; i<nodes[inode].nson; i++)
      nodes[inode].sons[i]=nodes[inode].sons[i+1];
   nodes[inode].nson--;

   tree.nnode++;
   NodeToBranch();
   if (!com.clock)
      nodes[tree.nnode-1].branch=bt*fmid;
   else
      nodes[tree.nnode-1].age=nodes[inode].age*(1-fclock);

   return(0);
}


#ifdef REALSEQUENCE


int MultipleGenes (FILE* fout, FILE*fpair[], double space[])
{
/* This does the separate analysis of multiple-gene data.
   Note that com.pose[] is not correct and so RateAncestor = 0 should be set
   in baseml and codeml.
*/
   int ig=0, j, ngene0, npatt0, lgene0[NGENE], posG0[NGENE+1];
   int nb = ((com.seqtype==1 && !com.cleandata) ? 3 : 1);
   
   if(com.ndata>1) error2("multiple data sets & multiple genes?");

   ngene0=com.ngene;  npatt0=com.npatt;
   FOR (ig, ngene0)   lgene0[ig]=com.lgene[ig];
   FOR (ig, ngene0+1) posG0[ig]=com.posG[ig];

   ig=0;
/*
   printf("\nStart from gene (1-%d)? ", com.ngene);
   scanf("%d", &ig); 
   ig--;
*/

   for ( ; ig<ngene0; ig++) {

      com.ngene=1;
      com.ls=com.lgene[0]= ig==0?lgene0[0]:lgene0[ig]-lgene0[ig-1];
      com.npatt =  ig==ngene0-1 ? npatt0-posG0[ig] : posG0[ig+1]-posG0[ig];
      com.posG[0]=0;  com.posG[1]=com.npatt;
      FOR (j,com.ns) com.z[j]+=posG0[ig]*nb;   com.fpatt+=posG0[ig];
      xtoy (com.piG[ig], com.pi, com.ncode);

      printf ("\n\nGene %2d  ls:%4d  npatt:%4d\n",ig+1,com.ls,com.npatt);
      fprintf(fout,"\nGene %2d  ls:%4d  npatt:%4d\n",ig+1,com.ls,com.npatt);
      fprintf(frst,"\nGene %2d  ls:%4d  npatt:%4d\n",ig+1,com.ls,com.npatt);
      fprintf(frst1,"%d\t%d\t%d",ig+1,com.ls,com.npatt);

#ifdef CODEML
      if(com.seqtype==CODONseq) {
         DistanceMatNG86(fout,fpair[0],fpair[1],fpair[2],0);
         if(com.codonf>=F1x4MG) com.pf3x4MG=com.f3x4MG[ig];
      }
#else
      if(com.fix_alpha)
         DistanceMatNuc(fout,fpair[0],com.model,com.alpha);
#endif

      if (com.runmode==0)  Forestry(fout);
#ifdef CODEML
      else if (com.runmode==-2) {
         if(com.seqtype==CODONseq) PairwiseCodon(fout,fpair[3],fpair[4],fpair[5],space);
         else                      PairwiseAA(fout,fpair[0]);
      }
#endif
      else                         StepwiseAddition(fout, space);

      FOR (j,com.ns) com.z[j]-=posG0[ig]*nb;
      com.fpatt-=posG0[ig];
      FPN(frst1);
   }
   com.ngene=ngene0;  com.npatt=npatt0;  com.ls=lgene0[ngene0-1];
   FOR(ig,ngene0)   com.lgene[ig]=lgene0[ig];
   FOR(ig,ngene0+1) com.posG[ig]=posG0[ig];
   return (0);
}

void printSeqsMgenes (void)
{
/* separate sites from different partitions (genes) into different files.
   called before sequences are coded.
   Note that this is called before PatternWeight and so posec or posei is used
   and com.pose is not yet allocated.
   In case of codons, com.ls is the number of codons.
*/
   FILE *fseq;
   char seqf[20];
   int ig, lg, i,j,h;
   int n31=(com.seqtype==CODONseq||com.seqtype==CODON2AAseq?3:1);

   puts("Separating sites in genes into different files.\n");
   for (ig=0, FPN(F0); ig<com.ngene; ig++) {
      for (h=0,lg=0; h<com.ls; h++)
         if(com.pose[h]==ig)
            lg++;
      sprintf (seqf, "Gene%d.seq\0", ig+1);
      if ((fseq=fopen(seqf,"w"))==NULL) error2("file creation err.");
      printf ("%d sites in gene %d go to file %s\n", lg, ig+1,seqf);

      fprintf (fseq, "%8d%8d\n", com.ns, lg*n31);
      for (j=0; j<com.ns; j++) {

         /* fprintf(fseq,"*\n>\n%s\n", com.spname[j]); */
         fprintf(fseq,"%-20s  ", com.spname[j]);
         if (n31==1)  {       /* nucleotide or aa sequences */
            FOR (h,com.ls)
		       if(com.pose[h]==ig)
			      fprintf(fseq, "%c", com.z[j][h]);
         }
         else {               /* codon sequences */
            FOR (h,com.ls)
               if(com.pose[h]==ig) {
                  FOR (i,3) fprintf(fseq,"%c", com.z[j][h*3+i]);
                  fputc(' ',fseq);
               }
         }
         FPN(fseq);
      }
      fclose (fseq);
   }
   return ;
}

void printSeqsMgenes2 (void)
{
/* This print sites from certain genes into one file.
   called before sequences are coded.
   In case of codons, com.ls is the number of codons.
*/
   FILE *fseq;
   char seqf[20]="newseqs";
   int ig, lg, i,j,h;
   int n31=(com.seqtype==CODONseq||com.seqtype==CODON2AAseq?3:1);
   
   int ngenekept=0;
   char *genenames[44]={"atpa", "atpb", "atpe", "atpf", "atph", "petb", "petg", "psaa",
"psab", "psac", "psaj", "psba", "psbb", "psbc", "psbd", "psbe",
"psbf", "psbh", "psbi", "psbj", "psbk", "psbl", "psbn", "psbt",
"rl14", "rl16", "rl2", "rl20", "rl36", "rpob", "rpoc", "rpod", "rs11",
"rs12", "rs14", "rs18", "rs19", "rs2", "rs3", "rs4", "rs7", "rs8",
"ycf4", "ycf9"};
   int wantgene[44]={0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                     0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
                     0, 0, 0, 0};
/*
for(ig=0,lg=0; ig<com.ngene; ig++) wantgene[ig]=!wantgene[ig];
*/

   if(com.ngene!=44) error2("ngene!=44");
   FOR(h,com.ls) { 
      printf("%3d",com.pose[h]); 
      if((h+1)%20==0) FPN(F0); if((h+1)%500==0) getchar();
   }
   matIout(F0,com.lgene,1,com.ngene);
   matIout(F0,wantgene,1,com.ngene);

   for(ig=0,lg=0; ig<com.ngene; ig++) 
      if(wantgene[ig]) { ngenekept++; lg+=com.lgene[ig]; }

   if((fseq=fopen(seqf,"w"))==NULL) error2("file creation err.");
   fprintf(fseq,"%4d %4d  G\nG  %d  ", com.ns, lg*n31, ngenekept);
   FOR(ig,com.ngene) if(wantgene[ig]) fprintf(fseq," %3d", com.lgene[ig]);
   FPN(fseq);

   for (j=0; j<com.ns; FPN(fseq),j++) {
      fprintf(fseq,"%-20s  ", com.spname[j]);
      if (n31==1)  {       /* nucleotide or aa sequences */
         FOR (h,com.ls)   
            if(wantgene[ig=com.pose[h]]) fprintf(fseq,"%c",com.z[j][h]);
      }
      else {               /* codon sequences */
         FOR (h,com.ls)
            if (wantgene[ig=com.pose[h]]) {
               FOR (i,3) fprintf(fseq,"%c", com.z[j][h*3+i]);
               fputc(' ', fseq);
            }
      }
   }
   FPN(fseq); 
   FOR(ig,com.ngene) if(wantgene[ig]) fprintf(fseq," %s", genenames[ig]);
   FPN(fseq);
   fclose (fseq);

   exit(0);
}

#endif   /* ifdef REALSEQUENCE */
#endif   /* ifdef TREESEARCH */
#endif   /* ifdef NODESTRUCTURE */



#ifdef PARSIMONY

void UpPassScoreOnly (int inode);
void UpPassScoreOnlyB (int inode);

static int *Nsteps, *chUB;   /* MM */
static char *Kspace, *chU, *NchU; 
/* Elements of chU are character states (there are NchU of them).  This 
   representation is used to speed up calculation for large trees.
   Bit operations on chUB are performed for binary trees
*/

void UpPassScoreOnly (int inode)
{
/* => VU, VL, & MM, theorem 2 */
   int ison, i, j;
   char *K=Kspace, maxK;  /* chMark (VV) not used in up pass */

   FOR (i,nodes[inode].nson)
      if (nodes[nodes[inode].sons[i]].nson>0)
          UpPassScoreOnly (nodes[inode].sons[i]);

   FOR (i,com.ncode) K[i]=0;
   FOR (i,nodes[inode].nson) 
      for (j=0,ison=nodes[inode].sons[i]; j<NchU[ison]; j++)
         K[(int)chU[ison*com.ncode+j]]++;
   for (i=0,maxK=0; i<com.ncode; i++)  if (K[i]>maxK) maxK=K[i];
   for (i=0,NchU[inode]=0; i<com.ncode; i++)
      if (K[i]==maxK)  chU[inode*com.ncode+NchU[inode]++]=(char)i;
   Nsteps[inode]=nodes[inode].nson-maxK;
   FOR (i, nodes[inode].nson)  Nsteps[inode]+=Nsteps[nodes[inode].sons[i]];
}

void UpPassScoreOnlyB (int inode)
{
/* uses bit operation, for binary trees only 
*/
   int ison1,ison2, i, change=0;

   FOR (i,nodes[inode].nson)
      if (nodes[nodes[inode].sons[i]].nson>0)
          UpPassScoreOnlyB (nodes[inode].sons[i]);

   ison1=nodes[inode].sons[0];  ison2=nodes[inode].sons[1];
   if ((chUB[inode]=(chUB[ison1] & chUB[ison2]))==0)
      { chUB[inode]=(chUB[ison1] | chUB[ison2]);  change=1; }
   Nsteps[inode]=change+Nsteps[ison1]+Nsteps[ison2];
}


double MPScore (double space[])
{
/* calculates MP score for a given tree using Hartigan's (1973) algorithm.
   sizeof(space) = nnode*sizeof(int)+(nnode+2)*ncode*sizeof(char).
   Uses Nsteps[nnode], chU[nnode*ncode], NchU[nnode].
   if(BitOperation), bit operations are used on binary trees.
*/
   int h,i, BitOperation,U[3],change;
   double score;

   Nsteps=(int*)space;
   BitOperation=(tree.nnode==2*com.ns-1 - (nodes[tree.root].nson==3));
   BitOperation=(BitOperation&&com.ncode<32);
   if (BitOperation)  chUB=Nsteps+tree.nnode;
   else {
      chU=(char*)(Nsteps+tree.nnode);
      NchU=chU+tree.nnode*com.ncode;  Kspace=NchU+tree.nnode;
   }
   for (h=0,score=0; h<com.npatt; h++) {
      FOR (i,tree.nnode) Nsteps[i]=0;
      if (BitOperation) { 
         FOR (i,com.ns)  chUB[i]=1<<(com.z[i][h]);
         UpPassScoreOnlyB (tree.root);
         if (nodes[tree.root].nson>2) {
            FOR (i,3) U[i]=chUB[nodes[tree.root].sons[i]];
            change=2;
            if (U[0]&U[1]&U[2]) change=0;
            else if (U[0]&U[1] || U[1]&U[2] || U[0]&U[2]) change=1;
            for (i=0,Nsteps[tree.root]=change; i<3; i++) 
               Nsteps[tree.root]+=Nsteps[nodes[tree.root].sons[i]];
       }
      }
      else {                   /* polytomies, use characters */
         FOR(i,com.ns)
            {chU[i*com.ncode]=(char)(com.z[i][h]); NchU[i]=(char)1; }
         for (i=com.ns; i<tree.nnode; i++)  NchU[i]=0;
         UpPassScoreOnly (tree.root);
      }
      score+=Nsteps[tree.root]*com.fpatt[h];
/*
printf("\nh %3d:    ", h+1);
FOR(i,com.ns) printf("%2d  ", com.z[i][h]);
printf(" %6d ", Nsteps[tree.root]);
if((h+1)%10==0) exit(1);
*/
   }

   return (score);
}

double RemoveMPNinfSites (double *nsiteNinf)
{
/* Removes parsimony-noninformative sites and return the number of changes 
   at those sites.
   Changes .z[], .fpatt[], .npatt, etc.
*/
   int  h,j, it, npatt0=com.npatt, markb[NCODE], gt2;
   double MPScoreNinf;

   for (h=0,com.npatt=0,MPScoreNinf=0,*nsiteNinf=0; h<npatt0; h++) {
      FOR (j, com.ncode) markb[j]=0;
      FOR (j, com.ns)  markb[(int)com.z[j][h]]++;
      for (j=0,it=gt2=0; j<com.ncode; j++)
         if (markb[j]>=2) { it++; gt2=1; }
      if (it<2) {                         /* non-informative */
       *nsiteNinf+=com.fpatt[h];
         FOR (j,com.ncode) if(markb[j]==1) MPScoreNinf+=com.fpatt[h];
         if (!gt2) MPScoreNinf-=com.fpatt[h];
      }
      else {
         FOR (j, com.ns) com.z[j][com.npatt]=com.z[j][h];
         com.fpatt[com.npatt++]=com.fpatt[h];
      }
   }
   return (MPScoreNinf);
}

#endif


#ifdef RECONSTRUCTION

static char *chMark, *chMarkU, *chMarkL; /* VV, VU, VL */
/* chMark, chMarkU, chMarkL (VV, VU, VL) have elements 0 or 1, marking
   whether the character state is present in the set */
static char *PATHWay, *NCharaCur, *ICharaCur, *CharaCur;
/* PATHWay, NCharaCur, ICharaCur, CharaCur are for the current 
   reconstruction.  
*/

int UpPass (int inode);
int DownPass (int inode);

int UpPass (int inode)
{
/* => VU, VL, & MM, theorem 2 */
   int n=com.ncode, i, j;
   char *K=chMark, maxK;   /* chMark (VV) not used in up pass */

   FOR (i,nodes[inode].nson)
      if (nodes[nodes[inode].sons[i]].nson>0) UpPass (nodes[inode].sons[i]);

   FOR (i, n) K[i]=0;
   FOR (i,nodes[inode].nson) 
      FOR (j, n)  if(chMarkU[nodes[inode].sons[i]*n+j]) K[j]++;
   for (i=0,maxK=0; i<n; i++)  if (K[i]>maxK) maxK=K[i];
   for (i=0; i<n; i++) {
      if (K[i]==maxK)         chMarkU[inode*n+i]=1; 
      else if (K[i]==maxK-1)  chMarkL[inode*n+i]=1;
   }
   Nsteps[inode]=nodes[inode].nson-maxK;
   FOR (i, nodes[inode].nson)  Nsteps[inode]+=Nsteps[nodes[inode].sons[i]];
   return (0);
}

int DownPass (int inode)
{
/* VU, VL => VV, theorem 3 */
   int n=com.ncode, i, j, ison;

   FOR (i,nodes[inode].nson) {
      ison=nodes[inode].sons[i];
      FOR (j,n) if (chMark[inode*n+j]>chMarkU[ison*n+j]) break;
      if (j==n) 
         FOR (j,n) chMark[ison*n+j]=chMark[inode*n+j];
      else 
         FOR (j,n)
            chMark[ison*n+j] = 
             (char)(chMarkU[ison*n+j]||(chMark[inode*n+j]&&chMarkL[ison*n+j]));
   }
   FOR (i,nodes[inode].nson)
      if (nodes[nodes[inode].sons[i]].nson>0) DownPass (nodes[inode].sons[i]);
   return (0);
}


int DownStates (int inode)
{
/* VU, VL => NCharaCur, CharaCur, theorem 4 */
   int i;

   FOR (i,nodes[inode].nson) 
      if (nodes[inode].sons[i]>=com.ns) 
         DownStatesOneNode (nodes[inode].sons[i], inode);
   return (0);
}

int DownStatesOneNode (int ison, int father)
{
/* States down inode, given father */
   char chi=PATHWay[father-com.ns];
   int n=com.ncode, j, in;

   if((in=ison-com.ns)<0) return (0);
   if (chMarkU[ison*n+chi]) {
      NCharaCur[in]=1;   CharaCur[in*n+0]=chi;
   }
   else if (chMarkL[ison*n+chi]) {
      for (j=0,NCharaCur[in]=0; j<n; j++) 
         if (chMarkU[ison*n+j] || j==chi) CharaCur[in*n+NCharaCur[in]++]=(char)j;
   }
   else {
      for (j=0,NCharaCur[in]=0; j<n; j++) 
         if (chMarkU[ison*n+j]) CharaCur[in*n+NCharaCur[in]++]=(char)j;
   }
   PATHWay[in]=CharaCur[in*n+(ICharaCur[in]=0)];
   FOR (j, nodes[ison].nson)  if (nodes[ison].sons[j]>=com.ns) break;
   if (j<nodes[ison].nson) DownStates (ison);

   return (0);
}

int InteriorStatesMP (int job, int h, int *nchange, char NChara[NS-1], 
    char Chara[(NS-1)*NCODE], double space[]);

int InteriorStatesMP (int job, int h, int *nchange, char NChara[NS-1], 
    char Chara[(NS-1)*NCODE], double space[])
{
/* sizeof(space) = nnode*sizeof(int)+3*nnode*ncode*sizeof(char)
   job: 0=# of changes; 1:equivocal states
*/
   int n=com.ncode, i,j;

   Nsteps=(int*)space;            chMark=(char*)(Nsteps+tree.nnode);
   chMarkU=chMark+tree.nnode*n;   chMarkL=chMarkU+tree.nnode*n;
   FOR (i,tree.nnode) Nsteps[i]=0;
   FOR (i,3*n*tree.nnode) chMark[i]=0;
   FOR (i,com.ns)  chMark[i*n+com.z[i][h]]=chMarkU[i*n+com.z[i][h]]=1;
   UpPass (tree.root);
   *nchange=Nsteps[tree.root];
   if (job==0) return (0);
   FOR (i,n) chMark[tree.root*n+i]=chMarkU[tree.root*n+i];
   DownPass (tree.root);
   FOR (i,tree.nnode-com.ns) 
      for (j=0,NChara[i]=0; j<n; j++) 
         if (chMark[(i+com.ns)*n+j])  Chara[i*n+NChara[i]++]=(char)j;
   return (0);     
}

int PathwayMP (FILE *fout, double space[])
{
/* Hartigan, JA.  1973.  Minimum mutation fits to a given tree. 
   Biometrics, 29:53-65.
*/
   char *pch=(com.seqtype==0?BASEs:AAs), visit[NS-1];
   int n=com.ncode, nid=tree.nbranch-com.ns+1, it, i,j,k, h, npath;
   int nchange, nchange0;
   char nodeb[NNODE], Equivoc[NS-1];

   PATHWay=(char*)malloc(nid*(n+3)*sizeof(char));
   NCharaCur=PATHWay+nid;  ICharaCur=NCharaCur+nid;  CharaCur=ICharaCur+nid;

   for (j=0,visit[i=0]=(char)(tree.root-com.ns); j<tree.nbranch; j++) 
     if (tree.branches[j][1]>=com.ns) 
        visit[++i]=(char)(tree.branches[j][1]-com.ns);
/*
   printf ("\nOrder in nodes: ");
   FOR (j, nid) printf ("%4d", visit[j]+1+com.ns); FPN(F0);
*/
   for (h=0; h<com.npatt; h++) {
      fprintf (fout, "\n%4d%6.0f  ", h+1, com.fpatt[h]);
      FOR (j, com.ns) fprintf (fout, "%c", pch[(int)com.z[j][h]]);
      fprintf (fout, ":  ");

      FOR (j,com.ns) nodeb[j]=(char)(com.z[j][h]);

      InteriorStatesMP (1, h, &nchange, NCharaCur, CharaCur, space); 
      ICharaCur[j=tree.root-com.ns]=0;  PATHWay[j]=CharaCur[j*n+0];
      FOR (j,nid) Equivoc[j]=(char)(NCharaCur[j]>1);
      DownStates (tree.root);

      for (npath=0; ;) {
         for (j=0,k=visit[nid-1]; j<NCharaCur[k]; j++) {
            PATHWay[k]=CharaCur[k*n+j]; npath++; 
            FOR (i, nid) fprintf (fout, "%c", pch[(int)PATHWay[i]]);
            fprintf (fout, "  ");

            FOR (i,nid) nodeb[i+com.ns]=PATHWay[i];
            for (i=0,nchange0=0; i<tree.nbranch; i++) 
            nchange0+=(nodeb[tree.branches[i][0]]!=nodeb[tree.branches[i][1]]);
            if (nchange0!=nchange) 
               { puts("\a\nerr:PathwayMP"); fprintf(fout,".%d. ", nchange0);}

         }
         for (j=nid-2; j>=0; j--) {
            if(Equivoc[k=visit[j]] == 0) continue;
            if (ICharaCur[k]+1<NCharaCur[k]) {
               PATHWay[k] = CharaCur[k*n + (++ICharaCur[k])];
               DownStates (k+com.ns);
               break;
            }
            else { /* if (next equivocal node is not ancestor) update node k */
               for (i=j-1; i>=0; i--) if (Equivoc[(int)visit[i]]) break;
               if (i>=0) { 
                  for (it=k+com.ns,i=visit[i]+com.ns; ; it=nodes[it].father)
                     if (it==tree.root || nodes[it].father==i) break;
                  if (it==tree.root)
                     DownStatesOneNode(k+com.ns, nodes[k+com.ns].father);
               }
            }
         }
         if (j<0) break;
       }
       fprintf (fout, " |%4d (%d)", npath, nchange);
   }   /* for (h) */
   free (PATHWay);
   return (0);
}

#endif



#if(BASEML || CODEML)


int BootstrapSeq (char* seqf)
{
/* This is called from within ReadSeq(), right after the sequences are read 
   and before the data are coded.
   jackknife if(lsb<com.ls && com.ngene==1).
   gmark[start+19] marks the position of the 19th site in that gene.
*/
   int iboot,nboot=com.bootstrap, h,is,ig,lg[NGENE]={0},j, start;
   int lsb=com.ls, n31=1,gap=10, gpos[NGENE];
   int *sites=(int*)malloc(com.ls*sizeof(int)), *gmark=NULL;
   FILE *fseq=(FILE*)gfopen(seqf,"w");
   enum {PAML=0, PAUP};
   char *dt=(com.seqtype==AAseq?"protein":"dna");
   char *paupstart="paupstart",*paupblock="paupblock",*paupend="paupend";
   int format=0;  /* 0: paml-phylip; 1:paup-nexus */

   printf("\nGenerating bootstrap samples in file %s\n", seqf);
   if(format==PAUP) {
      printf("%s, %s, & %s will be appended if existent.\n",
         paupstart,paupblock,paupend);
      appendfile(fseq,paupstart);
   }

   if(com.seqtype==CODONseq||com.seqtype==CODON2AAseq) { n31=3; gap=1; }
   if(sites==NULL) error2("oom in BootstrapSeq");
   if(com.ngene>1) {
      if(lsb<com.ls) error2("jackknife when #gene>1");
      if((gmark=(int*)malloc(com.ls*sizeof(int)))==NULL) 
         error2("oom in BootstrapSeq");

      FOR(ig,com.ngene)  com.lgene[ig]=gpos[ig]=0;
      FOR(h,com.ls)  com.lgene[com.pose[h]]++;
      for(j=0; j<com.ngene; j++) lg[j]=com.lgene[j];
      for(j=1; j<com.ngene; j++) com.lgene[j]+=com.lgene[j-1];

      if(noisy && com.ngene>1) {
         printf("Bootstrap uses stratefied sampling for %d partitions.",com.ngene);
         printf("\nnumber of sites in each partition: ");
         FOR(ig,com.ngene) printf(" %4d", lg[ig]);
         FPN(F0);
      }

      FOR(h,com.ls) {     /* create gmark[] */
         ig=com.pose[h];
         start=(ig==0?0:com.lgene[ig-1]);
         gmark[start + gpos[ig]++] = h;
      }
   }

   for (iboot=0; iboot<nboot; iboot++,FPN(fseq)) {
      if(com.ngene<=1)
         FOR(h,lsb) sites[h]=(int)(rndu()*com.ls);
      else {
         FOR(ig,com.ngene) {
            start=(ig==0?0:com.lgene[ig-1]);
            FOR(h,lg[ig])  sites[start+h] = gmark[start+(int)(rndu()*lg[ig])];
         }
      }

      /* print out the bootstrap sample */
      if(format==PAUP) {
         fprintf(fseq,"\n\n[Replicate # %d]\n", iboot+1);
         fprintf(fseq,"\nbegin data;\n");
         fprintf(fseq,"   dimensions ntax=%d nchar=%d;\n", com.ns, lsb*n31);
         fprintf(fseq,"   format datatype=%s missing=? gap=-;\n   matrix\n",dt);

         for(is=0;is<com.ns;is++,FPN(fseq)) {
            fprintf(fseq,"%-20s  ", com.spname[is]);
            FOR(h,lsb) {
               FOR(j,n31) fprintf(fseq,"%c", com.z[is][sites[h]*n31+j]);
               if((h+1)%gap==0) fprintf(fseq," ");
            }
         }

         fprintf(fseq, "   ;\nend;");
         /* site partitions */
         if(com.ngene>1) {
            fprintf(fseq, "\n\nbegin paup;\n");
            FOR(ig,com.ngene)
               fprintf(fseq, "   charset partition%-2d = %-4d - %-4d;\n", 
                  ig+1, (ig==0?1:com.lgene[ig-1]+1),com.lgene[ig]);
            fprintf(fseq, "end;\n");
         }
         appendfile(fseq, paupblock);
      }
      else {
         if(com.ngene==1) 
            fprintf(fseq,"%6d %6d\n", com.ns, lsb*n31);
         else {
            fprintf(fseq,"%6d %6d  G\nG %d  ", com.ns, lsb*n31, com.ngene);
            FOR(ig,com.ngene)
               fprintf(fseq," %4d", lg[ig]);
            FPN(fseq); FPN(fseq);
         }
         for(is=0;is<com.ns;is++,FPN(fseq)) {
            fprintf(fseq,"%-20s  ", com.spname[is]);
            FOR(h,lsb) {
               FOR(j,n31) fprintf(fseq,"%c", com.z[is][sites[h]*n31+j]);
               if((h+1)%gap==0) fprintf(fseq," ");
            }
         }
      }

      if(noisy && (iboot+1)%10==0) printf("\rdid sample #%d", iboot+1);
   }  /* for(iboot) */
   free(sites);  if(com.ngene>1) free(gmark);
   return(0);
}


int rell (FILE*flnf, FILE*fout, int ntree)
{
/* This implements three methods for tree topology comparison.  The first 
   tests the log likelihood difference using a normal approximation 
   (Kishino and Hasegawa 1989).  The second does approximate bootstrap sampling
   (the RELL method, Kishino and Hasegawa 1989, 1993).  The third is a 
   modification of the K-H test with a correction for multiple comparison 
   (Shimodaira and Hasegawa 1999) .
   The routine reads input from the file lnf.
   pattg[lgene] lists site patterns for gene ig for stratified sampling
  
   com.space[ntree*(npatt+nr+4)]: 
    lnf[ntree*npatt] lnL0[ntree] lnL[ntree*nr] pRELL[ntree] pSH[ntree] vdl[ntree]
*/
   char *line;
   int lline=16000, ntree0,ns0=com.ns, ls0,npatt0;
   int nr=10000, itree, h,ir,j,k, ig,lgene,npattg, mltree, *pattg, status=0;
   int nbtree, *btrees;
   double *lnf, *lnL0, *lnL, *pRELL, *pSH, *vdl, y, mdl, small=1e-5;

   fflush(fout);
   puts( "\nTree comparisons (Kishino & Hasegawa 1989; Shimodaira & Hasegawa 1999)");
   fputs("\nTree comparisons (Kishino & Hasegawa 1989; Shimodaira & Hasegawa 1999)\n",fout);
   fprintf(fout,"Number of replicates: %d\n", nr);

   fscanf(flnf,"%d%d%d", &ntree0, &ls0, & npatt0);
   if (ntree0!=ntree)  error2("rell: input data file strange.  Check.");
   if (ls0!=com.ls || npatt0!=com.npatt)
      error2("rell: input data file incorrect.");
   k = ntree*(com.npatt+nr+5)*sizeof(double) + com.ls*sizeof(int);
   if(com.sspace<k) {
      if(noisy) printf("resetting space to %d bytes in rell.\n",k);
      if((com.space=(double*)realloc(com.space,com.sspace=k))==NULL)
         error2("oom space");
      com.sspace=k;
   }
   lnf=com.space; lnL0=lnf+ntree*com.npatt; lnL=lnL0+ntree; pRELL=lnL+ntree*nr;
   pSH=pRELL+ntree; vdl=pSH+ntree; pattg=(int*)(vdl+ntree); btrees=pattg+ls0;
   lline = (com.seqtype==1 ? ns0*8 : ns0) + 100;
   lline = max2(16000, lline);
   if((line=(char*)malloc((lline+1)*sizeof(char)))==NULL) error2("oom rell");

   /* read lnf from file flnf, calculates lnL0[] & find ML tree */
   for(itree=0,mltree=0; itree<ntree; itree++) {
      printf("\r\tReading lnf for tree # %d", itree+1);
      fscanf(flnf,"%d",&k);
      if(k!=itree+1) 
         { printf("\nerr: lnf, reading tree %d.",itree+1); return(-1); }
      for(h=0,lnL0[itree]=0; h<com.npatt; h++) {
         fscanf (flnf, "%d%d%lf", &j, &k, &y);
         if(j!=h+1)
            { printf("\nlnf, patt %d.",h+1); return(-1); }
         fgets(line,lline,flnf);
         lnL0[itree]+=com.fpatt[h]*(lnf[itree*com.npatt+h]=y);
      }
      if(itree && lnL0[itree]>lnL0[mltree]) mltree=itree;
   }
   printf(", done.\n");
   free(line);
   /* calculates SEs (vdl) by sitewise comparison */
   printf("\r\tCalculating SEs by sitewise comparison");
   FOR(itree,ntree) {
      if(itree==mltree) { vdl[itree]=0; continue; }
      mdl=(lnL0[itree]-lnL0[mltree])/com.ls;
      for(h=0,vdl[itree]=0; h<com.npatt; h++) {
         y=lnf[itree*com.npatt+h]-lnf[mltree*com.npatt+h];
         vdl[itree]+=com.fpatt[h]*(y-mdl)*(y-mdl);
      }
      vdl[itree]=sqrt(vdl[itree]);
   }
   /* bootstrap resampling */
   if(com.ngene==1)
      for(h=0,k=0;h<com.npatt;h++) FOR(j,com.fpatt[h]) pattg[k++]=h;
   zero(pRELL,ntree); zero(pSH,ntree); zero(lnL,ntree*nr);
   for(ir=0; ir<nr; ir++) {
      for(ig=0; ig<com.ngene; ig++) {
         lgene=(ig?com.lgene[ig]-com.lgene[ig-1]:com.lgene[ig]);
         npattg=com.posG[ig+1]-com.posG[ig];
         if(com.ngene>1) {
            for(h=com.posG[ig],k=0;h<com.posG[ig+1];h++)
               FOR(j,com.fpatt[h]) pattg[k++]=h;
            if(k!=lgene) error2("rell: # sites for gene");
         }
         FOR(j,lgene) {
            h=pattg[(int)(lgene*rndu())];
            FOR(itree,ntree) lnL[itree*nr+ir]+=lnf[itree*com.npatt+h];
         }
      }

      /* y is the lnL for the best tree from replicate ir. */
      for(j=1,nbtree=1,btrees[0]=0,y=lnL[ir]; j<ntree; j++) {
         if(fabs(lnL[j*nr+ir]-y)<small) 
            btrees[nbtree++]=j;
         else if (lnL[j*nr+ir]>y)
            { nbtree=1; btrees[0]=j; y=lnL[j*nr+ir]; }
      }

      for(j=0; j<nbtree; j++) 
         pRELL[btrees[j]]+=1./(nr*nbtree);
      if((ir+1)%100==0) printf("\r\tBootstrapping.. replicate %6d / %d",ir+1,nr);
   }

   if(fabs(1-sum(pRELL,ntree))>1e-6) error2("sum pRELL != 1.");


   /* Shimodaira & Hasegawa correction (1999), working on lnL[ntree*nr] */
   for(j=0; j<ntree; j++)  /* step 3: centering */
      for(ir=0,y=sum(lnL+j*nr,nr)/nr; ir<nr; ir++) lnL[j*nr+ir]-=y;
   for(itree=0; itree<ntree; itree++) {  /* steps 4 & 5 */
      for(ir=0; ir<nr; ir++) {
         /* y is the best lnL in replicate ir.  The calculation is repeated */
         for(j=1,y=lnL[ir]; j<ntree; j++) 
            if(lnL[j*nr+ir]>y) y=lnL[j*nr+ir];
         if(y-lnL[itree*nr+ir]>lnL0[mltree]-lnL0[itree]) pSH[itree]+=1./nr;
      }
   }

   fprintf(fout,"\n%6s %12s %9s %9s%8s%10s%9s\n\n",
      "tree","li","Dli"," +- SE","pKH","pSH","pRELL");
   FOR(j,ntree) {
      mdl=lnL0[j]-lnL0[mltree]; 
      if(j==mltree || fabs(vdl[j])<1e-6) { y=-1; pSH[j]=-1; status=-1; }
      else y=1-CDFNormal(-mdl/vdl[j]);
      fprintf(fout,"%6d%c%12.3f %9.3f %9.3f%8.3f%10.3f%9.3f\n",
           j+1,(j==mltree?'*':' '),lnL0[j],mdl,vdl[j],y,pSH[j],pRELL[j]);
   }

fprintf(frst1,"%3d %12.2f",mltree+1, lnL0[mltree]);
for(j=0;j<ntree;j++) fprintf(frst1," %5.3f",pRELL[j]);
/*
for(j=0;j<ntree;j++) if(j!=mltree) fprintf(frst1,"%9.2f",pSH[j]);
*/

   fputs("\npKH: P value for KH normal test (Kishino & Hasegawa 1989)\n",fout);
   fputs("pRELL: RELL bootstrap proportions (Kishino & Hasegawa 1989)\n",fout);
   fputs("pSH: P value with multiple-comparison correction (MC in table 1 of Shimodaira & Hasegawa 1999)\n",fout);
   if(status) fputs("(-1 for P values means N/A)\n",fout);

   FPN(F0);
   return(0);
}

#endif




#ifdef LFUNCTIONS
#ifdef RECONSTRUCTION


void ListAncestSeq(FILE *fout, char *zanc);

void ListAncestSeq(FILE *fout, char *zanc)
{
/* zanc[nintern*com.npatt] holds ancestral sequences.
   Extant sequences are coded if cleandata.
*/
   int wname=15, j;

   fputs("\n\n\nList of extant and reconstructed sequences\n\n",fout);
   for(j=0;j<com.ns;j++,FPN(fout)) {
      fprintf(fout,"%-*s   ", wname,com.spname[j]);
      print1seq(fout,com.z[j],com.ls,com.cleandata, com.pose);
   }
   for(j=0;j<tree.nnode-com.ns;j++,FPN(fout)) {
      fprintf(fout,"node #%-*d  ", wname-5,com.ns+j+1);
      print1seq(fout, zanc+j*com.npatt, com.ls, 1, com.pose);
   }
}

int ProbSitePattern (double x[], double *lnL, double fhs[], double Sir[]);
int AncestralMarginal (FILE *fout, double x[], double fhs[], double Sir[]);
int AncestralJointPPSG2000 (FILE *fout, double x[], double fhs[]);


int ProbSitePattern (double x[], double *lnL, double fhs[], double ScaleC[])
{
/* This calculates probabilities for observing site patterns fhs[].  
   The following notes are for ncatG>1 and method = 0.  The routine calculates 
   the scale factor common to all site classes (ir), that is, the greatest of the 
   scale factors among the ir classes.  The common scale factors will be used in 
   scaling nodes[].conP for all site classes for all nodes in PostProbNode().  
   Small conP for some site classes will be essentially set to 0, which is fine.

   fhs[npatt]
   ScaleSite[npatt]

   Ziheng Yang, 7 Sept, 2001
*/
   int ig, i,k,h, ir;
   double fh, S, y=1;

   if(com.ncatG>1 && com.method==1) error2("don't need this?");
   if (SetParameters(x)) puts ("par err.");
   FOR(h,com.npatt) fhs[h]=0;
   if (com.ncatG<=1) {
      for (ig=0,*lnL=0; ig<com.ngene; ig++) {
         if(com.Mgene>1) SetPGene(ig, 1, 1, 0, x);
         ConditionalPNode (tree.root, ig, x);
         for (h=com.posG[ig]; h<com.posG[ig+1]; h++) {
            for (i=0; i<com.ncode; i++) 
               fhs[h] += com.pi[i]*nodes[tree.root].conP[h*com.ncode+i];
            *lnL -= log(fhs[h])*com.fpatt[h];
            if(com.NnodeScale) FOR(k,com.NnodeScale) 
               *lnL -= com.nodeScaleF[k*com.npatt+h]*com.fpatt[h];
         }
      }
   }
   else {
      for (ig=0; ig<com.ngene; ig++) {
         if(com.Mgene>1 || com.nalpha>1)
            SetPGene(ig, com.Mgene>1, com.Mgene>1, com.nalpha>1, x);
         for (ir=0; ir<com.ncatG; ir++) {
#ifdef CODEML
            if(com.seqtype==1 && com.NSsites /* && com.model */) IClass=ir;
#endif
            SetPSiteClass(ir, x);
            ConditionalPNode (tree.root, ig, x);

            for (h=com.posG[ig]; h<com.posG[ig+1]; h++) {
               for (i=0,fh=0; i<com.ncode; i++)
                  fh += com.pi[i]*nodes[tree.root].conP[h*com.ncode+i];
   
               if(com.NnodeScale) {
                  for(k=0,S=0; k<com.NnodeScale; k++)  S += com.nodeScaleF[k*com.npatt+h];
                  y=1;
                  if(ir==0)               ScaleC[h]=S;
                  else if(S<=ScaleC[h])   y=exp(S-ScaleC[h]);
                  else      /* change of scale factor */
                     { fhs[h] *= exp(ScaleC[h]-S);  ScaleC[h]=S; }
               }
               fhs[h] += com.freqK[ir]*fh*y;
            }
         }
      }
      for(h=0, *lnL=0; h<com.npatt; h++)  *lnL -= log(fhs[h])*com.fpatt[h];
      if(com.NnodeScale) FOR(h,com.npatt)    *lnL -= ScaleC[h]*com.fpatt[h];
   }
   /* if(noisy) printf("\nlnL = %12.6f from ProbSitePattern.\n", - *lnL); */

   return (0);
}


int fx_r(double x[], int np);
int updateconP(double x[], int inode);

int PostProbNode (int inode, double x[], double fhs[], double ScaleC[],
    double *lnL, double pChar1node[], char za[], double pnode[])
{
/* This calculates the full posterior distribution for node inode at each site.
   Below are special comments on gamma models and method = 0.

   Marginal reconstruction under gamma models, with complications arising from 
   scaling on large trees (com.NnodeScale) and the use of two iteration algorithms 
   (method).
   Z. Yang Sept 2001
   
   The algorithm is different depending on method, which makes the code clumsy.

   gamma method=0 or 2 (simultaneous updating):
      nodes[].conP overlap and get destroyed for different site classes (ir)
      The same for scale factors com.nodeScaleF. 
      fhs[npatt] and common scale factors ScaleC[npatt] are calculated for all 
      nodes before this routine is called.  The common scale factors are then 
      used to adjust nodes[].conP before they are summed across ir classes.

   gamma method=1 (one branch at a time):
      nodes[].conP (and com.nodeScaleF if node scaling is on) are separately 
      allocated for different site classes (ir), so that all info needed is
      available.  Use of updateconP() saves computation on large trees.
      Scale factor Sir[] is of size ncatG and reused for each h.
*/
   int n=com.ncode, i,k,h, ir,it=-1,best, ig;
   double fh, y,pbest, *Sir=ScaleC, S;

   *lnL=0;
   zero(pChar1node,com.npatt*n);

   /* nodes[].conP are reused for different ir, with or without node scaling */
   if (com.ncatG>1 && com.method!=1) {
      ReRootTree(inode);
      for (ig=0; ig<com.ngene; ig++) {
         if(com.Mgene>1 || com.nalpha>1)
            SetPGene(ig,com.Mgene>1,com.Mgene>1,com.nalpha>1,x);
         for (ir=0; ir<com.ncatG; ir++) {
#ifdef CODEML
            if(com.seqtype==1 && com.NSsites)  IClass=ir;
#endif
            SetPSiteClass(ir, x);
            ConditionalPNode (tree.root, ig, x);

            for (h=com.posG[ig]; h<com.posG[ig+1]; h++) {
               if(!com.NnodeScale) S=1;
               else {
                  for(k=0,S=0; k<com.NnodeScale; k++) 
                     S += com.nodeScaleF[k*com.npatt+h];
                  S=exp(S-ScaleC[h]);
               }
               for (i=0,fh=0; i<n; i++) {
                  y=com.freqK[ir]*com.pi[i]*nodes[tree.root].conP[h*n+i] * S;
                  fh += y;
                  pChar1node[h*n+i] += y ;
               }
            }
         }
      }
      for (h=0; h<com.npatt; h++) {
         for(i=0,y=0;i<n;i++) y += (pChar1node[h*n+i]/=fhs[h]);
         if (fabs(1-y)>1e-5) 
            error2("PostProbNode: sum!=1");
         for (i=0,best=-1,pbest=-1; i<n; i++)
            if (pChar1node[h*n+i]>pbest) { best=i; pbest=pChar1node[h*n+i]; }
         za[(inode-com.ns)*com.npatt+h]=(char)best;
         pnode[(inode-com.ns)*com.npatt+h]=pbest;
         *lnL-=log(fhs[h])*com.fpatt[h];
         if(com.NnodeScale) *lnL -= ScaleC[h]*com.fpatt[h];
      }
   }
   else {  /* all other cases: (alpha==0 || method==1) */
      FOR(i,tree.nnode) com.oldconP[i]=1;
      ReRootTree(inode);
      updateconP(x,inode);
      if (com.alpha==0 && com.ncatG<=1) { /* (alpha==0) (ngene>1 OK) */
         for (ig=0; ig<com.ngene; ig++) {
            if(com.Mgene==2 || com.Mgene==4)
               xtoy(com.piG[ig], com.pi, n);
            for (h=com.posG[ig]; h<com.posG[ig+1]; h++) {
               for (i=0,fh=0,pbest=0,best=-1; i<n; i++) {
                  y=com.pi[i]*nodes[tree.root].conP[h*n+i];
                  fh +=  y;
                  if (y>pbest)
                     { pbest=y; best=i; }
                  pChar1node[h*n+i]=y;
               }
               za[(inode-com.ns)*com.npatt+h]=(char)best;
               pnode[(inode-com.ns)*com.npatt+h]=(pbest/=fh);
               for (i=0; i<n; i++)  pChar1node[h*n+i]/=fh;
               *lnL -= log(fh)*(double)com.fpatt[h];
               FOR(i,com.NnodeScale) *lnL -= com.nodeScaleF[i*com.npatt+h]*com.fpatt[h];
            }
         }
      }
      else {  /* (ncatG>1 && method = 1)  This should work for NSsites? */
         for (ig=0; ig<com.ngene; ig++) {
            if(com.Mgene==2 || com.Mgene==4)
               xtoy(com.piG[ig], com.pi, n);
            for (h=com.posG[ig]; h<com.posG[ig+1]; h++) {
               if(com.NnodeScale)
                  for(ir=0,it=0; ir<com.ncatG; ir++) {  /* Sir[it] is the biggest */
                     for(k=0,Sir[ir]=0; k<com.NnodeScale; k++)
                        Sir[ir]+=com.nodeScaleF[ir*com.NnodeScale*com.npatt + k*com.npatt+h];
                     if(Sir[ir]>Sir[it]) it=ir;
                  }
               for (i=0,fh=0; i<n; i++)  {
                  for(ir=0; ir<com.ncatG; ir++) {
                     if(com.method==1)
                        y=nodes[tree.root].conP[ir*(tree.nnode-com.ns)*com.npatt*n+h*n+i];
                     else
                        y=nodes[tree.root].conP[h*n+i]; /* wrong right now */
                     y*=com.pi[i]*com.freqK[ir];
                     if(com.NnodeScale) y *= exp(Sir[ir]-Sir[it]);
   
                     pChar1node[h*n+i]+=y;
                     fh+=y;
                  }
               }
               for (i=0,best=0; i<n; i++)  {
                  pChar1node[h*n+i]/=fh;
                  if(i && pChar1node[h*n+best]<pChar1node[h*n+i]) best=i;
               }
               za[(inode-com.ns)*com.npatt+h]=(char)best;
               pnode[(inode-com.ns)*com.npatt+h]=pChar1node[h*n+best];
               *lnL -= log(fh)*com.fpatt[h];
               if(com.NnodeScale) *lnL -= Sir[it]*com.fpatt[h];
            }
         }
      }
   }
   return(0);
}


void getCodonNode1Site(char codon[], char zanc[], int inode, int site);

int AncestralMarginal (FILE *fout, double x[], double fhs[], double Sir[])
{
/* Ancestral reconstruction for each interior node.  This works under both 
   the one rate and gamma rates models.
   pnode[npatt*nid] stores the prob for the best chara at a node and site.
   The best character is kept in za[], coded as 0,...,n-1.
   The data may be coded (com.cleandata==1) or not (com.cleandata==0).
   Call ProbSitePatt() before running this routine.
   pMAPnode[NS-1], pMAPnodeA[] stores the MAP probabilities (accuracy)
   for a site and for the entire sequence, respectively.
 
   The routine PostProbNode calculates pChar1node[npatt*ncode], which stores 
   prob for each char at each pattern at each given node inode.  The rest of 
   the routine is to output the results in different ways.

   Deals with node scaling to avoid underflows.  See above 
   (Z. Yang, 2 Sept 2001)
*/
   char *pch=(com.seqtype==0?BASEs:(com.seqtype==2?AAs:BINs)), *zanc;
   char str[4]="",codon[2][4]={"   ","   "}, aa[4]="";
   int n=com.ncode, inode, ic=0,b[3],i,j,k1=-1,k2=-1,k3, lsc=com.ls;
   int h,hp,ig, best, oldroot=tree.root;
   int nid=tree.nnode-com.ns, nchange;
   double lnL=0, fh, y, pbest, *pChar1node, *pnode, p1=-1,p2=-1;
   double pMAPnode[NS-1], pMAPnodeA[NS-1], smallp=0.001;

   char coding=0, *bestAA=NULL;
   double pAA[21], *pbestAA=NULL, ns,na, nst,nat,S,N;
    /* bestAA[nid*npatt], pbestAA[nid*npatt]: 
       To reconstruct aa seqs using codon or nucleotide seqs, universal code */

   if(noisy) puts("Marginal reconstruction.");

   fprintf (fout,"\n(1) Marginal reconstruction of ancestral sequences ");
   fprintf (fout,"(eqn. 4 in Yang et al. 1995 Genetics 141:1641-1650).\n");
   pChar1node=(double*)malloc(com.npatt*n*sizeof(double));
   pnode=(double*)malloc((nid*com.npatt+1)*(sizeof(double)+sizeof(char)));
   if (pnode==NULL||pChar1node==NULL) error2("oom pnode");
   zanc=(char*)(pnode+nid*com.npatt);

#ifdef BASEML
   if(com.seqtype==0 && com.ls%3==0 && com.coding) { coding=1; lsc=com.ls/3; }
#endif
   if(com.seqtype==1) { coding=1; lsc=com.npatt; }
   if(coding==1) {
      if((pbestAA=(double*)malloc(nid*lsc*2*sizeof(double)))==NULL) 
         error2("oom pbestAA");
      bestAA=(char*)(pbestAA+nid*lsc);
   }

   if(SetParameters(x)) puts("par err."); 

   if(com.verbose>1) 
      fprintf(fout,"\nProb distribs at nodes, those with p < %.3f not listed\n",
         smallp);

   /* This loop reroots the tree at inode & reconstructs sequence at inode */
   for (inode=com.ns; inode<tree.nnode; inode++) {

      PostProbNode (inode, x, fhs, Sir, &lnL, pChar1node, zanc, pnode);
      if(noisy) printf ("\tNode %3d: lnL = %12.6f\n", inode+1, -lnL);

      /* print Prob distribution at inode if com.verbose>1 */
      if (com.verbose>1) {
         fprintf(fout,"\nProb distribution at node %d, by site\n",inode+1);
         if (com.ngene>1) fputs("\nSite (g) Freq  Data: \n\n",fout);
         else             fputs("\nSite   Freq   Data: \n\n",fout);
         for (h=0;h<com.ls;h++,FPN(fout)) {
            hp=com.pose[h];
            fprintf (fout,"%4d%7.0f   ", h+1,com.fpatt[hp]);
            print1site(fout,hp);   fputs(": ",fout);
            FOR(j,n)
               if(pChar1node[hp*n+j]>smallp) {
                  if (com.seqtype!=CODONseq) { str[0]=pch[j]; str[1]=0; }
                  else {
#ifdef CODEML
if(com.seqtype==1 &&(FROM61[j]<0||FROM61[j]>63)) { puts("strange 1"); getchar();}
                     getcodon(str,FROM61[j]);
#endif
                  }
                  fprintf(fout,"%s(%5.3f) ",str,pChar1node[hp*n+j]);
               }
         }
      }     /* if (verbose) */


      /* find the best amino acid for coding seqs */
#ifdef CODEML
      if(com.seqtype==CODONseq)
         FOR(h,com.npatt) {
            FOR(j,20) pAA[j]=0; 
            FOR(j,n) {
               i=GeneticCode[com.icode][FROM61[j]];
               pAA[i]+=pChar1node[h*n+j];
            }
            /* matout(F0,pAA,1,20); */
            for(j=0,best=0,pbest=0; j<20; j++) 
               if(pAA[j]>pbest) { pbest=pAA[j]; best=j; }
            bestAA[(inode-com.ns)*com.npatt+h]=(char)best;
            pbestAA[(inode-com.ns)*com.npatt+h]=pbest;
         }
#endif
      if(com.seqtype==0 && coding) { /* coding seqs analyzed by baseml */
         FOR(h,lsc) {  /* h-th codon */
            /* sums up probs for the 20 AAs for each node. Stop codons are 
               ignored, and so those probs are approxiamte. */
            for(j=0,y=0;j<20;j++) pAA[j]=0;
            FOR(k1,4) FOR(k2,4) FOR(k3,4) {
               ic=k1*16+k2*4+k3;
               b[0]=com.pose[h*3+0]*n+k1; 
               b[1]=com.pose[h*3+1]*n+k2; 
               b[2]=com.pose[h*3+2]*n+k3;
               fh=pChar1node[b[0]]*pChar1node[b[1]]*pChar1node[b[2]];
               if((ic=GeneticCode[com.icode][ic])==-1) y+=fh;
               else                                    pAA[ic]+=fh;
            }
            if(fabs(1-y-sum(pAA,20))>1e-6) error2("AncestralMarginal strange?");

            for(j=0,best=0,pbest=0; j<20; j++) 
               if(pAA[j]>pbest) { pbest=pAA[j]; best=j; }

            bestAA[(inode-com.ns)*com.ls/3+h]=(char)best;
            pbestAA[(inode-com.ns)*com.ls/3+h]=pbest/(1-y);
         }
      }
   }        /* for (inode), This closes the big loop */

   FOR(i,tree.nnode) com.oldconP[i]=0;
   ReRootTree(oldroot);

   if(com.seqtype==0 && coding) { /* coding seqs analyzed by baseml */
      fputs("\nBest amino acids reconstructed from nucleotide model.\n",fout);
      fputs("Prob at each node listed by amino acid (codon) site\n",fout);
      fputs("(Please ignore if not relevant)\n\n",fout);
      for(h=0;h<com.ls/3;h++,FPN(fout)) {
         fprintf(fout,"%4d ", h+1);
         FOR(j,com.ns) {
            getCodonNode1Site(codon[0], NULL, j, h);
            Codon2AA(codon[0], aa, com.icode, &i);
            fprintf(fout," %s(%c)",codon[0],AAs[i]);
         }
         fprintf(fout,": ");
         for (j=0; j<tree.nnode-com.ns; j++) {
            fprintf(fout," %1c (%5.3f)",
               AAs[(int)bestAA[j*com.ls/3+h]],pbestAA[j*com.ls/3+h]);
         }
      }
   }

   /* calculate accuracy measures */
   zero(pMAPnode,nid);  fillxc(pMAPnodeA, 1., nid);
   for (inode=0; inode<tree.nnode-com.ns; inode++) {
      FOR (h,com.npatt) {
         pMAPnode[inode]+=com.fpatt[h]*pnode[inode*com.npatt+h]/com.ls;
         pMAPnodeA[inode]*=pow(pnode[inode*com.npatt+h], com.fpatt[h]);
      }
   }

   fputs("\n\nProb of best character at each node, listed by site ",fout);
   if (com.ngene>1) fputs("\n\nSite (g) Freq  Data: \n",fout);
   else             fputs("\n\nSite   Freq   Data: \n",fout);

   FOR (h, com.ls) {
      hp=com.pose[h];
      fprintf(fout,"\n%4d ",h+1);
      if (com.ngene>1) {  /* which gene the site is from */
         for(ig=1;ig<com.ngene;ig++) if(hp<com.posG[ig]) break;
         fprintf(fout,"(%d)",ig);
      }
      fprintf(fout," %5.0f   ", com.fpatt[hp]);
      print1site(fout,hp);   fputs(": ",fout);
      FOR(j,nid) {
         if (com.seqtype!=CODONseq)
            fprintf(fout,"%c(%5.3f) ",
               pch[(int)zanc[j*com.npatt+hp]],pnode[j*com.npatt+hp]);
         else {
            ;
#ifdef CODEML
            i=GeneticCode[com.icode][ic=FROM61[(int)zanc[j*com.npatt+hp]]];
            fprintf(fout," %s %1c %5.3f (%1c %5.3f)",
               getcodon(str,ic),AAs[i],pnode[j*com.npatt+hp], 
               AAs[(int)bestAA[j*com.npatt+hp]],pbestAA[j*com.npatt+hp]);
#endif
         }
      }
      if(noisy && (h+1)%100000==0) printf("\r\tprinting, %d sites done", h+1);
   }
   if(noisy && h>=100000) printf("\n");

   /* Map changes onto branches 
      k1 & k2 are the two characters; p1 and p2 are the two probs. */
   fputs("\n\nSummary of changes along branches.\n",fout);
   fputs("Check root of tree for directions of change.\n",fout);
   if(!com.cleandata && com.seqtype==1) 
      fputs("Counts of n & s are incorrect along tip branches with ambiguity data.\n",fout);
   for(j=0;j<tree.nbranch;j++,FPN(fout)) {
      inode=tree.branches[j][1];  nchange=0;
      fprintf(fout,"\nBranch %d:%5d..%-2d",j+1,tree.branches[j][0]+1,inode+1);
      if(inode<com.ns) fprintf(fout," (%s) ",com.spname[inode]);

      if(coding) {
         lsc=(com.seqtype==1?com.ls:com.ls/3);
         for (h=0,nst=nat=0; h<lsc; h++)  {
            getCodonNode1Site(codon[0], zanc, inode, h);
            getCodonNode1Site(codon[1], zanc, tree.branches[j][0], h);
            difcodonNG(codon[0],codon[1], &S, &N, &ns,&na, 0, com.icode);
            nst+=ns;  nat+=na;
         }
         fprintf(fout," (n=%4.1f s=%4.1f)",nat,nst);
      }
      fprintf(fout,"\n\n");

      for(h=0;h<com.ls;h++) {
         hp=com.pose[h];
         if (com.seqtype!=CODONseq) {
            if(inode<com.ns)
               k2=(com.cleandata?pch[(int)com.z[inode][hp]]:com.z[inode][hp]);
            else {
               k2=pch[(int)zanc[(inode-com.ns)*com.npatt+hp]]; 
               p2=pnode[(inode-com.ns)*com.npatt+hp];
            }
            k1=pch[(int)zanc[(tree.branches[j][0]-com.ns)*com.npatt+hp]];
            p1=pnode[(tree.branches[j][0]-com.ns)*com.npatt+hp];
         }
         else {
#ifdef CODEML
            if(inode<com.ns) {
               if(!com.cleandata)
                  { Codon2AA(com.z[inode]+hp*3,aa,com.icode,&k2); k2=AAs[k2];}
               else
                  k2=AAs[GeneticCode[com.icode][FROM61[(int)com.z[inode][hp]]]];
            }
            else {
               k2=AAs[(int)bestAA[(inode-com.ns)*com.npatt+hp]];
               p2=pbestAA[(inode-com.ns)*com.npatt+hp];
            }
            k1=AAs[(int)bestAA[(tree.branches[j][0]-com.ns)*com.npatt+hp]];
            p1=pbestAA[(tree.branches[j][0]-com.ns)*com.npatt+hp];
#endif
         }
         if(k1==k2) continue;
         fprintf(fout,"\t%4d ",h+1);

#ifdef SITELABELS
         if(sitelabels) fprintf(fout," %5s   ",sitelabels[h]);
#endif
         if(inode<com.ns) fprintf(fout,"%c %.3f -> %1c\n",k1,p1,k2);
         else             fprintf(fout,"%c %.3f -> %1c %.3f\n",k1,p1,k2,p2);
         ++nchange;
      }
   }

   ListAncestSeq(fout, zanc);
   fprintf(fout,"\n\nOverall accuracy of the %d ancestral sequences:", nid);
   matout2(fout,pMAPnode, 1, nid, 9,5);  fputs("for a site.\n",fout);
   matout2(fout,pMAPnodeA, 1, nid, 9,5); fputs("for the sequence.\n", fout);

   /* best amino acid sequences from codonml */
#ifdef CODEML
   if(com.seqtype==1) {
      fputs("\n\nAmino acid sequences inferred by codonml.\n",fout);
      if(!com.cleandata) 
         fputs("Results unreliable for sites with alignment gaps.\n",fout);
      for(inode=0; inode<nid; inode++) {
         fprintf(fout,"\nNode #%-10d  ",com.ns+inode+1);
         for(h=0;h<com.ls;h++) {
            fprintf(fout,"%c",AAs[(int)bestAA[inode*com.npatt+com.pose[h]]]);
            if((h+1)%10==0) fputc(' ',fout);
         }
      }
      FPN(fout);
   }
#endif
   ChangesSites(fout, coding, zanc);

   free(pnode); free(pChar1node);
   if(coding) free(pbestAA);
   return (0);
}



void getCodonNode1Site(char codon[], char zanc[], int inode, int site)
{
/* this is used to retrive the codon from a codon sequence for codonml 
   or coding sequence in baseml, used in ancestral reconstruction
   zanc has ancestral sequences
   site is codon site
*/
   int i, hp;

   FOR(i,3) codon[i]=-1;  /*  to force crashes */
   if(com.seqtype==CODONseq) {
      hp=com.pose[site];
#ifdef CODEML
      if(inode>=com.ns)
         getcodon(codon,FROM61[(int)zanc[(inode-com.ns)*com.npatt+hp]]);
      else {
         if(com.cleandata)
            getcodon(codon,FROM61[(int)com.z[inode][hp]]);
         else
            FOR(i,3) codon[i]=com.z[inode][hp*3+i];
      }
#endif
   }
   else {      /* baseml coding reconstruction */
      if(inode>=com.ns)
         FOR(i,3)
            codon[i]=BASEs[(int)zanc[(inode-com.ns)*com.npatt+com.pose[site*3+i]]];
      else {
         if(com.cleandata)
            FOR(i,3) codon[i]=BASEs[(int)com.z[inode][com.pose[site*3+i]]];
         else  
            FOR(i,3) codon[i]=com.z[inode][com.pose[site*3+i]];
      }
   }

}

int ChangesSites(FILE*frst, int coding, char *zanc)
{
/* this lists and counts changes at sites from reconstructed ancestral sequences
   com.z[] has the data, and zanc[] has the ancestors
   For codon sequences (codonml or baseml with com.coding), synonymous and 
   nonsynonymous changes are counted separately.
   Added in Nov 2000.
*/
   char *pch=(com.seqtype==0?BASEs:(com.seqtype==2?AAs:BINs));
   char codon[2][4]={"   ","   "};
   int  h,hp,inode,k1,k2,d, ls1=com.ls;
   double S,N,Sd,Nd, S1,N1,Sd1,Nd1, b,btotal=0, p,C;

   if(com.seqtype==0 && coding) ls1=com.ls/3;
   if(com.seqtype==CODONseq && !com.cleandata) {
      fputs("\a\nCounts of n & s are incorrect at sites with ambiguity data\n",frst);
   }
   if(coding) {
      fputs("\n\nChanges at sites (syn nonsyn).\n\n",frst);
      /*      fputs("syn and nonsyn changes at each site\n",frst1); */

      fprintf(frst1,"\nList of sites with changes according to ancestral reconstruction\n");
      fprintf(frst1,"Suzuki-Gojobori (1999) style test\n\n");

      for(inode=0; inode<tree.nnode; inode++)  
         if(inode!=tree.root) btotal+=nodes[inode].branch;
      for(h=0; h<ls1; h++) {
         fprintf(frst,"%4d ",h+1);
         for(inode=0,S=N=Sd=Nd=0; inode<tree.nnode; inode++) {
            if(inode==tree.root) continue;
            b=nodes[inode].branch;
            getCodonNode1Site(codon[0], zanc, nodes[inode].father, h);
            getCodonNode1Site(codon[1], zanc, inode, h);

            difcodonNG(codon[0],codon[1],&S1,&N1,&Sd1,&Nd1, 0, com.icode);
            S+=S1*b/btotal; N+=N1*b/btotal;
            if(Sd1||Nd1) {
               Sd+=Sd1;  Nd+=Nd1;
               fprintf(frst," %3s.%3s ",codon[0],codon[1]);
            }
         }
         b=S+N; S/=b;  N/=b;
         fprintf(frst,"(S N: %7.3f%7.3f Sd Nd: %6.1f %5.1f)\n", S*3,N*3,Sd,Nd);
         fprintf(frst1,"%4d S N: %7.3f%7.3f Sd Nd: %6.1f %5.1f ", h+1,S*3,N*3,Sd,Nd);
         if(Sd+Nd) {
            if(Nd/(Sd+Nd)<N) {
               for(d=0,p=0,C=1; d<=Nd; d++) {
                  p += C*pow(N,d)*pow(1-N,Sd+Nd-d);
                  C *= (Sd+Nd-d)/(d+1);
               }
               fprintf(frst1," - p =%6.3f %s", p,(p<.01?"**":(p<.05?"*":"")));
            }
            else {
               for(d=0,p=0,C=1; d<=Sd; d++) {
                  p += C*pow(S,d)*pow(1-S,Sd+Nd-d);
                  C *= (Sd+Nd-d)/(d+1);
               }
               fprintf(frst1," + p =%6.3f %s", p,(p<.01?"**":(p<.05?"*":"")));
            }
         }
         fprintf(frst1,"\n");
      }
   }
   else {  /* noncoding nucleotide or aa sequences */
      fputs("\n\nCounts of changes at sites\n\n",frst);
      for(h=0;h<com.ls;h++) {
         hp=com.pose[h];
         fprintf(frst,"%4d ",h+1);
         for(inode=0,d=0;inode<tree.nnode;inode++) {
            if(inode==tree.root) continue;
            k1=pch[(int) zanc[(nodes[inode].father-com.ns)*com.npatt+hp] ];
            if(inode<com.ns) {
               if(com.cleandata) k2=pch[(int)com.z[inode][hp]];
               else              k2=com.z[inode][hp];
            }
            else  
               k2=pch[(int) zanc[(inode-com.ns)*com.npatt+hp] ];
            if(k1!=k2) {
               d++;
               fprintf(frst," %c%c", k1,k2);
            }
         }
         fprintf(frst," (%d)\n", d);
      }
   }
   return(0);
}



double *PMatTips;
char *nodeChar, *ancSeq;  /* C_z(i) in PPSG2000; conP stores L_z(i) */
/* nodeChar[(z-com.ns)*com.npatt*n+h*n+i] is C_z(i), probability 
   of the best reconstruction upto node z, given that z's father
   has character i.
   nodeChar[nintern*n*com.npatt]
*/

void UpPassPPSG2000 (int inode, int igene, double x[])
{
/* Steps 2 & 4 in PPSG2000, modified from ConditionalPNode().  
   It calculates the L's in PPSG for node inode (z), storing them in 
   nodes[inode].conP.  
   nodes[inode].conP[h*n+i] and nodeChar[(inode-com.ns)*com.npatt*n+h*n+i] are 
   prob and chara at inode, given that father has ichar.
   inode has daughter nodes xnode (that is, x and y in PPSG).
   ichar is for father, and jchar is for inode.
   nodes[root].conP[h] is the prob for best reconstruction at the site

   Need more thoughts about ambiguity data (error in running stewart.aa).
*/
   int n=com.ncode, ichar,jchar, jbest=-1, xnode,ixnode,h, i,k;
   double t=-1,pj,pjbest=-1;

   FOR(ixnode,nodes[inode].nson) 
      if(nodes[xnode=nodes[inode].sons[ixnode]].nson>0) /* if son xnode is not a tip */
         UpPassPPSG2000(xnode,igene, x);

#if CODEML
if(com.seqtype==1 && com.hkyREV) puts("\ado UpPassPPSG2000 for hkyREV.");
#endif

   if(inode!=tree.root)
      GetPMatBranch(PMat, x, nodes[inode].branch*com.rgene[igene]*_rateSite,inode);

   for(h=com.posG[igene]; h<com.posG[igene+1]; h++) { /* to save space */
      for(ichar=0; ichar<(inode!=tree.root?n:1); ichar++) { /* ichar for father */
         for(jchar=0; jchar<n; jchar++) { /* jchar for inode, for finding best*/
            pj=(inode!=tree.root?PMat[ichar*n+jchar]:com.pi[jchar]);
            for(ixnode=0; ixnode<nodes[inode].nson; ixnode++) {
               xnode=nodes[inode].sons[ixnode];  /* this loops through inode's sons */
               if(com.cleandata && nodes[xnode].nson<1)  /* xnode is tip */
                  pj*=PMatTips[xnode*n*n+jchar*n+com.z[xnode][h]];
               else                              /* xnode is internal node or tip */
                  pj*=nodes[xnode].conP[h*n+jchar];
            }
            if(jchar==0 || pjbest<pj)
               { pjbest=pj; jbest=jchar; } 
         }
         nodes[inode].conP[h*n+ichar]=pjbest; 
         nodeChar[(inode-com.ns)*com.npatt*n+h*n+ichar]=(char)jbest;
      }
   }

   if(com.NnodeScale && com.nodeScale[inode]) {
      for(i=0,k=0; i<tree.nnode; i++)   /* k-th node for scaling */
         if(i==inode) break;  else if(com.nodeScale[i]) k++;
      for(h=com.posG[igene]; h<com.posG[igene+1]; h++)
         for(i=0;i<com.ncode;i++)
            nodes[inode].conP[h*com.ncode+i] /= exp(com.nodeScaleF[k*com.npatt+h]);
   }
}

void DownPassPPSG2000(int inode, int igene)
{
/* this reads out the best chara for inode from nodeChar[] into ancSeq[].
   c0 is best chara for father (if inode is not root).
*/
   int i,ison, h;
   char c0=0;

   for(h=com.posG[igene]; h<com.posG[igene+1]; h++) {
      if(inode!=tree.root) 
         c0=ancSeq[(nodes[inode].father-com.ns)*com.npatt+h];
      ancSeq[(inode-com.ns)*com.npatt+h]
         =nodeChar[(inode-com.ns)*com.npatt*com.ncode+h*com.ncode+c0];
   }
   FOR(i,nodes[inode].nson)
      if(nodes[ison=nodes[inode].sons[i]].nson>1) 
         DownPassPPSG2000(ison,igene);
}


int AncestralJointPPSG2000 (FILE *fout, double x[], double fhs[])
{
/* Z. Yang, 8 June 2000
   Joint ancestral reconstruction, taking character states for all nodes at a 
   site as one entity, based on the algorithm of Pupko et al. (2000 Mol Biol Evol 
   17:890-896).

   fhs[]: fh[] for each site pattern
   nodes[].conP[] are used to store the probabilities (L's in PSSG), so that 
   conditoinal probability values are destroyed.
   ancSeq[nintern*com.npatt] stores the ancestral seqs, from UpPassPPSG2000.

   Node scaling uses the com.nodeScaleF[] calculated in ConditionalPNode().  
   In that case, fhs[] is the remaining probs after the scaling factors are 
   taken out.
*/
   char *pch=(com.seqtype==0?BASEs:(com.seqtype==2?AAs:BINs)),codon[4]="";
   int n=com.ncode,nintern=tree.nnode-com.ns, i,h,hp,ig, newspace;
   double *psites;
   
   if(noisy) puts("Joint reconstruction.");
   nodeChar=(char*)malloc(nintern*com.npatt*(n+1)*sizeof(char));
   ancSeq=nodeChar+nintern*com.npatt*n;
   if(nodeChar==NULL) error2("oom nodeChar in AncestralJointPPSG2000");
   newspace = max2(com.ns*n*n,com.npatt)*sizeof(double);
   if(com.cleandata && com.sspace<newspace) {
      com.sspace = newspace;
      if((com.space=(double*)realloc(com.space,com.sspace))==NULL)
         error2("oom PPSG");
      if(noisy && com.sspace>1000000) 
         printf("\nspace reset to %d bytes in Ancestral.\n",com.sspace);
   }

   PMatTips=psites=com.space;
   for(ig=0; ig<com.ngene; ig++) {
      if(com.Mgene>1) SetPGene(ig,1,1,0,x);

      FOR(i,com.ns)
         GetPMatBranch(PMatTips+i*n*n, x, nodes[i].branch*com.rgene[ig]*_rateSite,i);
      UpPassPPSG2000(tree.root, ig, x);
      for (h=com.posG[ig]; h<com.posG[ig+1]; h++)
         psites[h]=nodes[tree.root].conP[h*n+0]/fhs[h];
      DownPassPPSG2000(tree.root,ig); /* enumerate best characters */
   }

   fputs("\n\n(2) Joint reconstruction of ancestral sequences ", fout);
   fputs("(eqn. 2 in Yang et al. 1995 Genetics 141:1641-1650), using ",fout);
   fputs("the algorithm of Pupko et al. (2000 Mol Biol Evol 17:890-896).\n",fout);

   fputs("\nListed by site, reconstruction (prob.)\n",fout);
   if (com.ngene>1) fputs("\n\nSite (g) Freq  Data: \n",fout);
   else             fputs("\n\nSite   Freq   Data: \n",fout);

   /* Output ancestral sequences */
   for (h=0; h<com.ls; h++) {
      hp=com.pose[h];
      for(ig=1;ig<com.ngene;ig++) if (hp<com.posG[ig]) break;
      ig--;

      fprintf (fout,"\n%4d ", h+1);
      if (com.ngene>1) fprintf(fout,"(%d) ", ig+1);
      fprintf (fout," %6.0f  ",com.fpatt[hp]);
      print1site(fout,hp); fputs(": ",fout);
      FOR(i,nintern) {
         if(com.seqtype==1) {
#ifdef CODEML
            fprintf(fout,"%s ",getcodon(codon,FROM61[(int)ancSeq[i*com.npatt+hp]]));
#endif
         }
         else
            fprintf(fout,"%c",pch[(int)ancSeq[i*com.npatt+hp]]);
      }
      fprintf(fout,"  (%.4f)",psites[hp]);
      if(noisy && (h+1)%100000==0) printf("\r\t%d sites done", h+1);
   }  /* for (h) */
   if(noisy && h>100000) printf("\n");

   ListAncestSeq(fout, ancSeq);
   free(nodeChar);
   return (0);
}


int AncestralSeqs (FILE *fout, double x[])
{
/* Ancestral sequence reconstruction using likelihood (Yang et al. 1995).
   Marginal works with constant rate and variable rates among sites.
   Joint works only with constant rate among sites.
*/
   double *fhs, lnL, *ScaleC=NULL;  /* collected scale factors */

   if(com.Mgene==1) {
      puts("When Mgene=1, use RateAncestor = 0."); return(-1);
   }
   if (tree.nnode==com.ns) 
      { puts("\nNo ancestral nodes to reconstruct..\n");  return(0); }
   if (noisy) printf ("\nReconstructed ancestral states go into file rst.\n");
   fprintf(fout, "\nAncestral reconstruction by %sML.\n",
          (com.seqtype==0?"BASE":(com.seqtype==1?"CODON":"AA")));
   FPN(fout);  OutaTreeN(fout,1,1);  FPN(fout);  FPN(fout);
   OutaTreeN(fout,0,0);  FPN(fout);  FPN(fout);
   OutaTreeB (fout);     FPN(fout);

   fputs("\ntree with node labels for Rod Page's TreeView\n",fout);
   OutaTreeN(fout,1,2);  FPN(fout);

   fprintf (fout, "\nNodes %d to %d are ancestral\n", com.ns+1,tree.nnode);
   if ((fhs=(double*)malloc(com.npatt*sizeof(double)))==NULL) error2("oom fhs");
   if(com.NnodeScale && com.ncatG>1)
      if((ScaleC=(double*)malloc(max2(com.npatt,com.ncatG) *sizeof(double)))==NULL) 
         error2("oom ScaleC in AncestralSeqs");

   if (com.ncatG<=1 || com.method!=1)
      ProbSitePattern (x, &lnL, fhs, ScaleC);

   if (com.alpha)
      puts("Rates are variable among sites, marginal reconstructions only.");
   if(!com.verbose) fputs("Constant sites not listed for verbose=0\n",fout);
   if(!com.cleandata) fputs("Unreliable at sites with alignment gaps\n",fout);

   AncestralMarginal(fout,x,fhs,ScaleC);  fflush(fout);

   if(com.ncatG<=1 && tree.nnode>com.ns+1 
      && com.cleandata && com.clock==0 && com.NnodeScale==0)
      AncestralJointPPSG2000 (fout, x, fhs);

   FPN(fout);
   free (fhs);
   if(com.NnodeScale && com.ncatG>1) free(ScaleC);
   return (0);
}

#endif


int SetNodeScale(int inode);
int NodeScale(int inode, int pos0, int pos1);

void InitializeNodeScale(void)
{
/* This allocates memory to hold scale factors for nodes and also decide on the 
   nodes for scaling by calling SetNodeScale().  
   The scaling node is chosen before the iteration by counting the number of 
   nodes visited in the post-order tree travesal algorithm (see the routine 
   SetNodeScale).
   See Yang (2000 JME 51:423-432) for details.
   The memory required is  com.NnodeScale*com.npatt*sizeof(double).
*/
   int i,nS;

   if(com.clock>=5) return;

   com.NnodeScale=0;
   com.nodeScale=(char*)realloc(com.nodeScale,tree.nnode*sizeof(char));
   if(com.nodeScale==NULL) error2("oom");
   FOR(i,tree.nnode) com.nodeScale[i]=0;
   SetNodeScale(tree.root);
   nS=com.NnodeScale*com.npatt;
   if(com.conPSiteClass) nS*=com.ncatG;
   if(com.NnodeScale) {
      if((com.nodeScaleF=(double*)realloc(com.nodeScaleF,nS*sizeof(double)))==NULL)
         error2("oom nscale");
      FOR(i,nS) com.nodeScaleF[i]=0;

      if(noisy) {
         printf("\n%d node(s) used for scaling (Yang 2000 J Mol Evol 51:423-432):\n",com.NnodeScale);
         FOR(i,tree.nnode) if(com.nodeScale[i]) printf(" %2d",i+1);
         FPN(F0);
      }
   }
}


int SetNodeScale (int inode)
{
/* This marks nodes for applying scaling factors when calculating f[h].
*/
   int i,ison, d=0, every;

   if(com.seqtype==0)       every=50;   /* baseml */
   else if(com.seqtype==1)  every=10;    /* codonml */
   else                     every=30;    /* aaml */

   FOR(i,nodes[inode].nson) {
      ison=nodes[inode].sons[i];
      d+=(nodes[ison].nson ? SetNodeScale(ison) : 1);
   }
   if(inode!=tree.root && d>every)
      { com.nodeScale[inode]=1;  d=1; com.NnodeScale++; }
   return(d);
}


int NodeScale (int inode, int pos0, int pos1)
{
/* scale to avoid underflow
*/
   int h,k,j, n=com.ncode;
   double t, smallw=1e-12;

   for(j=0,k=0; j<tree.nnode; j++)   /* k-th node for scaling */
      if(j==inode) break;
      else if(com.nodeScale[j]) k++;

   for(h=pos0; h<pos1; h++) {
      for(j=0,t=0;j<n;j++)
         if(nodes[inode].conP[h*n+j]>t) t=nodes[inode].conP[h*n+j];

      if(t<1e-300) {
         for(j=0;j<n;j++)  nodes[inode].conP[h*n+j]=1;  /* both 0 and 1 fine */
         com.nodeScaleF[k*com.npatt+h]=-800;  /* this is problematic? */
      }
      else {  
         for(j=0;j<n;j++)  nodes[inode].conP[h*n+j]/=t;
         com.nodeScaleF[k*com.npatt+h]=log(t);
      }
   }
   return(0);
}



#if (BASEML || CODEML)
int fx_r(double x[], int np);

int lfunRates (FILE* fout, double x[], int np)
{
/* for dG, AdG or similar non-parametric models
   This distroys com.fhK[], and in return,
   fhK[<npatt] stores rates for conditional mean (re), and 
   fhK[<2*npatt] stores the most probable rate category number.
   fhs[npatt] stores fh=log(fh).
*/
   int  ir,il,it, h,hp,j, nscale=1, direction=-1;
   double lnL=0,fh,fh1, t, re,mre,vre, b1[NCATG],b2[NCATG],*fhs;

   if (noisy) printf("\nEstimated rates for sites go into file %s\n",ratef);
   if (SetParameters(x)) puts ("par err. lfunRates");

   fprintf(fout, "\nEstimated rates for sites from %sML.\n",
          (com.seqtype==0?"BASE":(com.seqtype==1?"CODON":"AA")));
   OutaTreeN(fout,1,1); FPN(fout);
   fprintf (fout,"\nFrequencies and rates for categories (K=%d)", com.ncatG);
   matout (fout, com.freqK, 1, com.ncatG);
   matout (fout, com.rK, 1, com.ncatG);
   if (com.rho) {
      fprintf(fout,"\nTransition prob matrix over sites");
      matout2(fout,com.MK,com.ncatG,com.ncatG,8,4);
   }

   if((fhs=(double*)malloc(com.npatt*sizeof(double)))==NULL) error2("oom fhs");
   fx_r(x,np);
   if(com.NnodeScale) {
      FOR(h,com.npatt) {
         for(ir=1,it=0; ir<com.ncatG; ir++)
            if(com.fhK[ir*com.npatt+h]>com.fhK[it*com.npatt+h]) it=ir;
         t=com.fhK[it*com.npatt+h];
         lnL-=com.fpatt[h]*t;
         for(ir=0; ir<com.ncatG; ir++)
            com.fhK[ir*com.npatt+h]=exp(com.fhK[ir*com.npatt+h]-t);
      }
   }
   for(h=0;h<com.npatt;h++) {
      for(ir=0,fhs[h]=0; ir<com.ncatG; ir++)
         fhs[h]+=com.freqK[ir]*com.fhK[ir*com.npatt+h];
   }

   if (com.rho==0) {     /* dG model */
      if(com.verbose>1) {
         fprintf(fout,"\nPosterior probabilities for site classes\n\n");
         for (h=0; h<com.ls; h++,FPN(fout)) {
            fprintf(fout, "Site %4d ", h+1);
            hp=com.pose[h];
            for (ir=0; ir<com.ncatG; ir++)
               fprintf(fout, " %9.4f", com.freqK[ir]*com.fhK[ir*com.npatt+hp]/fhs[hp]);
         }
      }

      fprintf(fout,"\nSite Freq  Data    Rate (posterior mean & category)\n\n");
      for (h=0,mre=vre=0; h<com.npatt; h++) {
         for (ir=0,it=0,t=re=0; ir<com.ncatG; ir++) {
            fh1=com.freqK[ir]*com.fhK[ir*com.npatt+h];
            if(fh1>t)  { t=fh1; it=ir; }
            re+=fh1*com.rK[ir];
         }
         lnL -= com.fpatt[h]*log(fhs[h]);

         re/=fhs[h];
         mre+=com.fpatt[h]*re/com.ls;  vre+=com.fpatt[h]*re*re/com.ls;
         com.fhK[h]=re;                com.fhK[com.npatt+h]=it+1.;
      }
      vre-=mre*mre;
      FOR(h,com.ls) {
         hp=com.pose[h];
         fprintf(fout,"%4d %5.0f  ",h+1,com.fpatt[hp]);  print1site(fout,hp);
         fprintf(fout," %8.3f%6.0f\n", com.fhK[hp],com.fhK[com.npatt+hp]);
         if(noisy && (h+1)%100000==0) printf("\r\tprinting, %d sites done", h+1);
      }
      if(noisy && h>100000) printf("\n");
      if(!com.cleandata || com.ngene>1) 
         fputs("\n(With ambiguity or multigene data, ignore Nexp.)\n",fout);
   }
   else {      /* Auto-dGamma model */
      fputs("\nSite Freq  Data  Rates\n\n",fout);
      h = (direction==1?com.ls-1:0);
      for (il=0,mre=vre=0; il<com.ls; h-=direction,il++) {
         hp=com.pose[h];
         if (il==0)
            FOR(ir,com.ncatG) b1[ir]=com.fhK[ir*com.npatt+hp];
         else {
            for (ir=0; ir<com.ncatG; ir++) {
               for (j=0,fh=0; j<com.ncatG; j++)
                  fh+=com.MK[ir*com.ncatG+j]*b1[j];
               b2[ir] = fh*com.fhK[ir*com.npatt+hp];
            }
            xtoy (b2, b1, com.ncatG);
         }
         if ((il+1)%nscale==0)
            { fh=sum(b1,com.ncatG); abyx(1/fh,b1,com.ncatG); lnL-=log(fh); }

         for (ir=0,it=-1,re=fh1=t=0; ir<com.ncatG; ir++) {
            re+=com.freqK[ir]*b1[ir]*com.rK[ir];
            fh1+=com.freqK[ir]*b1[ir];
            if (b1[ir]>t) {it=ir; t=b1[ir]; }
         }
         re/=fh1;  mre+=re/com.ls;   vre+=re*re/com.ls;

         fprintf(fout,"%4d %5.0f  ",h+1,com.fpatt[hp]);  print1site(fout,hp);
         fprintf(fout," %8.3f%6.0f\n", re,it+1.);
      }  /* for(il) */
      vre-=mre*mre;
      for (ir=0,fh=0; ir<com.ncatG; ir++)  fh += com.freqK[ir]*b1[ir];
      lnL -= log(fh);
   }
   if (noisy) printf ("lnL=%14.6f\n", -lnL);
   fprintf (fout,"\nlnL=%14.6f\n", -lnL);
   if(com.ngene==1) {
      fprintf (fout,"\nmean(r^)=%9.4f  var(r^)=%9.4f", mre, vre);
      fprintf (fout,"\nAccuracy of rate prediction: corr(r^,r) =%9.4f\n", 
               sqrt(com.alpha*vre));
   }
   free(fhs);
   return (0);
}


double lfunAdG (double x[], int np)
{
/* Auto-Discrete-Gamma rates for sites
   See notes in lfundG().
*/
   int  nscale=1, h,il, ir, j, FPE=0;
   int  direction=-1;  /* 1: n->1;  -1: 1->n */
   double lnL=0, b1[NCATG], b2[NCATG], fh;

   NFunCall++;
   fx_r(x,np);
   if(com.NnodeScale)
      FOR(h,com.npatt) {
         fh=com.fhK[0*com.npatt+h];
         lnL-=fh*com.fpatt[h];
         for(ir=1,com.fhK[h]=1; ir<com.ncatG; ir++) 
            com.fhK[ir*com.npatt+h]=exp(com.fhK[ir*com.npatt+h]-fh);
      }
   h = (direction==1?com.ls-1:0);
   for (il=0; il<com.ls; h-=direction,il++) {
      if (il==0)
         FOR(ir,com.ncatG) b1[ir]=com.fhK[ir*com.npatt+com.pose[h]];
      else {
         for (ir=0; ir<com.ncatG; ir++) {
            for (j=0,fh=0; j<com.ncatG; j++)
               fh+=com.MK[ir*com.ncatG+j]*b1[j];
            b2[ir]=fh*com.fhK[ir*com.npatt+com.pose[h]];
         }
         xtoy(b2,b1,com.ncatG);
      }
      if((il+1)%nscale==0) {
         fh=sum(b1,com.ncatG);
         if(fh<1e-90) {
            if(!FPE) {
               FPE=1; printf ("h,fh%6d %12.4e\n", h+1,fh);
               print1site(F0,h);  FPN(F0);
            }
            fh=1e-300;
         }
         abyx(1/fh,b1,com.ncatG); lnL-=log(fh);
      }
   }
   for (ir=0,fh=0; ir<com.ncatG; ir++)  fh+=com.freqK[ir]*b1[ir];
   lnL-=log(fh);
   return (lnL);
}

#endif




#if defined(CODEML)

int Set_UVR_BranchSite (int iclass, int branchlabel)
{
/*
   For branch&site models, this uses IClass to set U&V&Root to the right place 
   in the 5 sets of U&V&Root vectors (w0b,w0f,w1b,w1f,w2f):
       iclass=0: w0b (0), w0f (1)
       iclass=1: w1b (2), w1f (3)
       iclass=2: w0b (0), w2f (4)
       iclass=3: w1b (2), w2f (4)
   
   For Joe's branch&site models (ncatG=3 for NSselection, and 2 or 3 for NSdiscrete):
       iclass=0: w0b (0), w0f (1)
       iclass=1: w1b (2), w1f (3)
       iclass=2: w2b (4), w2f (5)
*/
   int iUVR=0, iUVR_YN02[4][2]={{0,1}, {2,3}, {0,4}, {2,4}};

   if(com.model==0 || com.NSsites==0) error2("should not be here.");

   if(com.model<=NSbranch2) iUVR=iUVR_YN02[iclass][branchlabel];
   else                     iUVR=iclass*2+branchlabel;
   U=_UU[iUVR]; V=_VV[iUVR]; Root=_Root[iUVR];
   return (iUVR);
}

#endif

#if (defined(BASEML) || defined(CODEML))

int GetPMatBranch (double Pt[], double x[], double t, int inode)
{
/* P(t) for branch leading to inode, called by routines ConditionalPNode()
   and AncestralSeq() in baseml and codeml.
   x[] is not used by baseml.
*/
   int updateUVRoot=0;
   int iUVR=0, nUVR=6, iw;
   static int iUVR_old=-1;
   static double t_save=-1, w_save=-1;
   double *pkappa, space[16]={0}, small=1e-50;

#if (defined CODEML)
   double *pomega=com.pomega; /* x+com.ntime+com.nrgene+com.nkappa; */

   pkappa=(com.hkyREV||com.codonf>=FMutSel3x4?x+com.ntime+com.nrgene:&com.kappa);

   if(com.seqtype==1 && com.NSsites && com.model) { /* branch&site models */
      iUVR = Set_UVR_BranchSite (IClass, (int)nodes[inode].label);
      if(iUVR != iUVR_old) { iUVR_old=iUVR; UVRootChanged=1; }
   }
   else if (com.seqtype==CODONseq && BayesEB==2 && com.model>1) { /* BEB for A&C */
      iw=(int)nodes[inode].label;
      if(fabs(com.pomega[iw]-w_save)>small)
         { updateUVRoot=1;  w_save=com.pomega[iw]; }
      if(fabs(Qfactor_NS-Qfactor_NS_branch[iw])>small)
         { updateUVRoot=1; Qfactor_NS=Qfactor_NS_branch[iw]; }
      if(updateUVRoot)
         EigenQc(0,-1,NULL,NULL,NULL,Root,U,V, pkappa, w_save, Pt);

   }
   else if (com.seqtype==CODONseq && (com.model==1 ||com.model==2) 
         && com.nbtype<=nUVR) { /* branch model, also for AAClasses */
      iUVR=(int)nodes[inode].label;
      U=_UU[iUVR]; V=_VV[iUVR]; Root=_Root[iUVR];
   }
   else if (com.seqtype==CODONseq && com.model) { /* AAClass model */
      if(com.aaDist==AAClasses) {
         com.pomega = PointOmega(x+com.ntime, -1, inode, -1);
         updateUVRoot = 1;
      }
      else {  /* Is this ever reached?  Looks strange? */
         updateUVRoot=(com.omega!=nodes[inode].omega);
         if(com.NSsites && Qfactor_NS!=Qfactor_NS_branch[(int)nodes[inode].label]) {
            Qfactor_NS=Qfactor_NS_branch[(int)nodes[inode].label]; 
            updateUVRoot=1; 
         }
      }
      if(updateUVRoot)
         EigenQc(0,-1,NULL,NULL,NULL,Root,U,V, pkappa, nodes[inode].omega, Pt);
   }

   if(UVRootChanged || fabs(t-t_save)>1e-15) {
      if (com.seqtype==AAseq && com.model==Poisson)
         PMatJC69like(Pt,t,com.ncode);
      else
         PMatUVRoot(Pt,t,com.ncode,U,V,Root);
      t_save=t;  UVRootChanged=0;
   }

#elif (defined BASEML)
   if (com.model<=K80)
      PMatK80(Pt, t, (com.nhomo==2?nodes[inode].kappa:com.kappa));
   else {
      if (com.nhomo==2)
         EigenTN93(com.model,nodes[inode].kappa, -1, com.pi,&nR,Root,Cijk);
      else if (com.nhomo>2) /* need kappa on each node if fix_kappa ==0 */
         EigenTN93(com.model,nodes[inode].kappa, -1, nodes[inode].pi,&nR,Root,Cijk);
      if(com.model<=REV||com.model==REVu)  
         PMatCijk(Pt,t);
      else {
         QUNREST(NULL, Pt, x+com.ntime+com.nrgene, com.pi);
         matexp (Pt, t, 4, 10, space);
      }
   }
#endif

   return(0);
}

#endif


void print_lnf_site (int h, double logfh)
{
#if(defined BASEML || defined CODEML)      
   fprintf(flnf,"\n%6d %6d %6d %6.0f %16.10f %12.4f  ", h+1, global_pose[0][h]+1, global_pose[1][h], com.fpatt[h],logfh,com.ls*exp(logfh));
   print1site(flnf,h);
#endif
}

int fx_r(double x[], int np);

double lfundG (double x[], int np)
{
/* likelihood function for site-class models.
   This deals with scaling for nodes to avoid underflow if(com.NnodeScale).
   The routine calls fx_r() to calculate com.fhK[], which holds log{f(x|r)} 
   when scaling or f(x|r) when not.  Scaling factors are set and used for each 
   site class (ir) to calculate log(f(x|r).  When scaling is used, the routine 
   converts com.fhK[] into f(x|r), by collecting scaling factors into lnL.  
   The rest of the calculation then becomes the same and relies on f(x|r).  
   Check notes in fx_r.
   This is also used for NSsites models in codonml.  
   Note that scaling is used between fx_r() and ConditionalPNode()
*/
   int h,ir, it, FPE=0;
   double lnL=0, fh=0,t;
   char codon[4]="   ";

   NFunCall++;
   fx_r(x,np);
   if(com.NnodeScale) { /* com.fhK[] has log{f(x|r}.  Note the scaling for nodes */
      for(h=0; h<com.npatt; h++) {
         for(ir=1,it=0; ir<com.ncatG; ir++) /* select term for scaling */
            if(com.fhK[ir*com.npatt+h]>com.fhK[it*com.npatt+h]) it=ir;

         t=com.fhK[it*com.npatt+h];
         for(ir=0,fh=0; ir<com.ncatG; ir++)
            fh += com.freqK[ir]*exp(com.fhK[ir*com.npatt+h]);
         fh = log(fh);
         lnL -= com.fpatt[h] * fh;
         if (com.print<0) 
         {
         print_lnf_site(h,fh);
         global_like[global_ig][h]=fh;
         }
      }
   }
   else 
      for(h=0; h<com.npatt; h++) {
         if (com.fpatt[h]==0) continue;
         for(ir=0,fh=0; ir<com.ncatG;ir++) 
            fh += com.freqK[ir]*com.fhK[ir*com.npatt+h];

         if(fh<=0) {
            if(!FPE) {
               FPE=1;  matout(F0,x,1,np);
               printf("\nlfundG: h=%4d  fhK=%9.6e\ndata: ", h+1, fh);
               print1site(F0,h);  FPN(F0);
            }
            fh=1e-300;
         }
         fh=log(fh);
         lnL-=fh*com.fpatt[h];
         if (com.print<0)
         {
         print_lnf_site(h,fh);
         global_like[global_ig][h]=fh;
         }
      }

   return(lnL);
}


int SetPSiteClass(int iclass, double x[])
{
/* This sets parameters for the iclass-th site class
   This is used by ConditionalPNode() and also updateconP in both algorithms
   For method=0 and 1.
*/
   int k=com.nrgene+!com.fix_kappa;
   double *pkappa=NULL, *xcom=x+com.ntime;

   _rateSite=com.rK[iclass];
#if CODEML
   IClass=iclass;
   pkappa=(com.hkyREV||com.codonf>=FMutSel3x4?xcom+com.nrgene:&com.kappa);
   if(com.seqtype==CODONseq && com.NSsites) {
      _rateSite=1;
      if (!com.model) {
         if(com.aaDist) {
            if(com.aaDist<10)       com.pomega=xcom+k+com.ncatG-1+2*iclass;
            else if(com.aaDist==11) com.pomega=xcom+k+com.ncatG-1+4*iclass;
            else if(com.aaDist==12) com.pomega=xcom+k+com.ncatG-1+5*iclass;
         }
         EigenQc(0,-1,NULL,NULL,NULL,Root,U,V, pkappa,com.rK[iclass],PMat);
      }
   }
#endif
   return (0);
}

extern int prt, Locus, Ir;

int fx_r (double x[], int np)
{
/* This calculates f(x|r) if(com.NnodeScale==0) or log{f(x|r)} 
   if(com.NnodeScale>0), that is, the (log) probability of observing data x 
   at a site, given the rate r or dN/dS ratio for the site.  This is used by 
   the discrete-gamma models in baseml and codeml as well as the NSsites models 
   in codeml.  
   The results are stored in com.fhK[com.ncatG*com.npatt].
   This deals with underflows with large trees using global variables 
   com.nodeScale and com.nodeScaleF[com.NnodeScale*com.npatt].
*/
   int  h, ir, i,k, ig, FPE=0;
   double fh, smallw=1e-12; /* for testing site class with w=0 */

   if(!BayesEB)
      if(SetParameters(x)) puts("\npar err..");

   for(ig=0; ig<com.ngene; ig++) { /* alpha may differ over ig */
      if(com.Mgene>1 || com.nalpha>1)
         SetPGene(ig, com.Mgene>1, com.Mgene>1, com.nalpha>1, x);
      for(ir=0; ir<com.ncatG; ir++) {
         if(ir && com.conPSiteClass) {  /* shift com.nodeScaleF & conP */
            if(com.NnodeScale) com.nodeScaleF+=com.npatt*com.NnodeScale;
            for(i=com.ns;i<tree.nnode;i++)
               nodes[i].conP += (tree.nnode-com.ns)*com.ncode*com.npatt;
         }
         SetPSiteClass(ir,x);
         ConditionalPNode(tree.root,ig, x);

         for (h=com.posG[ig]; h<com.posG[ig+1]; h++) {
            if(com.fpatt[h]==0) continue;
            for (i=0,fh=0; i<com.ncode; i++)
               fh += com.pi[i]*nodes[tree.root].conP[h*com.ncode+i];
            if (fh<=0) {
               if(fh<-1e-10 /* && !FPE */) { /* note that 0 may be o.k. here */
                  FPE=1; matout(F0,x,1,np);
                  printf("\nfx_r: h=%d  r=%d fhK=%.5e", h+1,ir+1,fh);
                  if(com.seqtype==0||com.seqtype==2)
                     { printf("Data: ");  print1site(F0,h);  FPN(F0); }
               }
               fh = 1e-300;
            }
            if(!com.NnodeScale)
               com.fhK[ir*com.npatt+h]=fh;
            else
               for(k=0,com.fhK[ir*com.npatt+h]=log(fh); k<com.NnodeScale; k++)
                  com.fhK[ir*com.npatt+h]+=com.nodeScaleF[k*com.npatt+h];
         }  /* for (h) */
      }     /* for (ir) */

      if(com.conPSiteClass) {  /* shift pointers conP back */
         if(com.NnodeScale) com.nodeScaleF-=(com.ncatG-1)*com.NnodeScale*com.npatt;
         for(i=com.ns;i<tree.nnode;i++)
            nodes[i].conP-=(com.ncatG-1)*(tree.nnode-com.ns)*com.ncode*com.npatt;
      }
   }  /* for(ig) */
   return(0);
}


double lfun (double x[], int np)
{
/* likelihood function for models of one rate for all sites including 
   Mgene models.
*/
   int  h,i,k, ig, FPE=0;
   double lnL=0, fh;

   NFunCall++;
   if(SetParameters(x)) puts ("\npar err..");
   for(ig=0; ig<com.ngene; ig++) {
      if(com.Mgene>1) SetPGene(ig,1,1,0,x);
      ConditionalPNode (tree.root, ig, x);
      for(h=com.posG[ig]; h<com.posG[ig+1]; h++) {
         if(com.fpatt[h]==0) continue;
         for(i=0,fh=0; i<com.ncode; i++)
            fh += com.pi[i]*nodes[tree.root].conP[h*com.ncode+i];
         if(fh<=0) {
            if(fh<-1e-5 && noisy) printf("\nfh = %.6f negative\n",fh);
            if(!FPE) {
               FPE=1;  matout(F0,x,1,np);
               printf("\alfun: h=%4d  fh=%9.6e\nData: ", h+1,fh);
               print1site(F0,h);  FPN(F0);
            }
            fh=1e-80;
         }
         fh=log(fh);
         FOR(k,com.NnodeScale) fh+=com.nodeScaleF[k*com.npatt+h];

         lnL-=fh*com.fpatt[h];

         if (com.print<0)
         {
         print_lnf_site(h,fh);
         global_like[global_ig][h]=fh;
         }
      }
   }

   return (lnL);
}




int print1site (FILE*fout,int h)
{
/* This print out one site in the sequence data, com.z[].  It may be the h-th 
   site in the original data file or the h-th pattern.
   The data are coded if com.cleandata==1 or not if otherwise.
*/
   char str[4]="";
   char *pch=(com.seqtype==0?BASEs:(com.seqtype==2?AAs:BINs));
   int i, ib,nb=(com.seqtype==CODONseq?3:1), iaa=0,ic=0;

   if(!com.cleandata) 
      FOR(i,com.ns) {
         FOR(ib,nb) fprintf(fout,"%c", com.z[i][h*nb+ib]);
#ifdef CODEML
         if(nb==3)  {
            Codon2AA(com.z[i]+h*nb, str, com.icode, &iaa);
            fprintf(fout," (%c) ",AAs[iaa]);
         }
#endif
      }
   else {
      FOR(i,com.ns) {
         if(nb==1) fprintf(fout,"%c", pch[(int)com.z[i][h]]);
#ifdef CODEML
         else {
            ic=FROM61[(int)com.z[i][h]];  iaa=GeneticCode[com.icode][ic];
            fprintf(fout,"%s (%c) ", getcodon(str,ic), AAs[iaa]);
         }
#endif
      }
   }
   return(0);
}
   

int InitConditionalPNode (void)
{
/* set conditional probability at tips of the tree, when com.cleandata==0.
   Need testing if sequences in the data are ancestors.
*/
   static int times=0;
   int n=com.ncode, is,j,k,h;
   int i=0, nb[3]={-1,-1,-1},ib[3][4]={{-1,-1,-1}}, i0=0,i1=0,i2=0,ic=0,nsense=0;
   char *pch=(com.seqtype==0?BASEs:(com.seqtype==2?AAs:BINs));

#if (defined BASEML || defined CODEML)
   if(com.clock<5) 
      FOR(is,com.ns) nodes[is].conP=com.conP0 + is* n*com.npatt;
#endif

   for (is=0;is<com.ns;is++) {
      zero(nodes[is].conP, com.npatt*n);
      for (h=0;h<com.npatt;h++) {
         if(com.seqtype==CODONseq) {
#ifdef CODEML
            for (h=0;h<com.npatt;h++) {
               FOR(i,3)  NucListall(com.z[is][h*3+i],&nb[i],ib[i]);
               for(i0=0,nsense=0; i0<nb[0]; i0++) FOR(i1,nb[1]) FOR(i2,nb[2]){
                  ic=ib[0][i0]*16+ib[1][i1]*4+ib[2][i2];         
                  ic=FROM64[ic];
                  if(ic>-1) { nsense++;  nodes[is].conP[h*n+ic]=1; }
               }
            }
            if(nsense==0) {
               printf("\nseq. %d pattern %d",is+1,h+1);
               error2("InitconP: stop codon");
            }
#endif
         }
         else {
            k=strchr(pch,com.z[is][h])-pch; 
            if(k<0) { printf("Character %d\n",com.z[is][h]); exit(-1); }
            if(k<n)
               nodes[is].conP[h*n+k]=1;
            else if (com.seqtype==AAseq)
               FOR(k,n) nodes[is].conP[h*n+k]=1;
            else if (com.seqtype==BASEseq)
               FOR(j,nBASEs[k]) 
                  nodes[is].conP[h*n+ (strchr(BASEs,EquateNUC[k][j])-BASEs)]=1;
         }
      }  /* for (h) */
   }     /* for (is) */
   return(0);
}



#if(defined MINIMIZATION)

/* November, 1999, Minimization branch by branch */
int noisy_minbranches;
double *space_minbranches, *varb_minbranches, e_minbranches;

double minbranches(double xcom[], int np);
int lfunt(double t, int a,int b,double x[],double *l, double space[]);
int lfuntdd(double t, int a,int b,double x[], double *l,double*dl,double*ddl,
    double space[]);
int lfunt_SiteClass(double t, int a,int b,double x[],double *l,double space[]);
int lfuntdd_SiteClass(double t, int a,int b,double x[],
    double *l,double*dl,double*ddl,double space[]);

int minB (FILE*fout, double *lnL,double x[],double xb[][2],double e0, double space[])
{
/* This calculates lnL for given values of common parameters by optimizing 
   branch lengths, cycling through them.
   Z. Yang, November 1999
   This calls minbranches to optimize branch lengths and ming2 to 
   estimate other paramters.
   At the end of the routine, there is a call to lfun to restore nodes[].conP.
   Returns variances of branch lengths in space[].

   return value: 0 convergent;  -1: not convergent.
*/
   int ntime0=com.ntime, fix_blength0=com.fix_blength;
   int status=0, i, npcom=com.np-com.ntime;
   double *xcom=x+com.ntime, lnL0= *lnL, dl, e=1e-5;
   double (*xbcom)[2]=xb+ntime0;
   int small_times=0, max_small_times=100, ir,maxr=(npcom?200:1);
   double small_improvement=0.001;
   char timestr[64];

   varb_minbranches=space;
   i = (3*com.ncode*com.ncode + (com.conPSiteClass) * 4*com.npatt) *sizeof(double);
   if((space_minbranches=(double*)malloc(i))==NULL)  error2("oom minB");
   if(com.ntime==0) error2("minB: should not come here");

   if(*lnL<=0)  *lnL=com.plfun(x,com.np);
   e=e_minbranches= (npcom ? 1.0 : e0);
   com.ntime=0; com.fix_blength=2;
#if(CODEML)
   if(com.NSsites==0) com.pomega=xcom+com.nrgene+!com.fix_kappa;
#endif

   for(ir=0; (npcom==0||com.method) && ir<maxr; ir++) {
      if(npcom) {
         if(noisy>2) printf("\n\nRound %da: Paras (%d) (e=%.6g)",ir+1,npcom,e);
         ming2(NULL,lnL,com.plfun,NULL,xcom, xbcom, space,e,npcom);
         if(noisy>2) {
            FPN(F0); FOR(i,npcom) printf("%12.6f",xcom[i]);
            printf("%8s%s\n", "", printtime(timestr));
         }
      }

      noisy_minbranches=noisy;
      if(noisy>2)
         printf("\nRound %db: Blengths (%d, e=%.6g)\n",ir+1,tree.nbranch,e_minbranches);

      *lnL=minbranches(xcom, -1);
      FOR(i,tree.nnode)  if(i!=tree.root) x[nodes[i].ibranch]=nodes[i].branch;
      if(noisy>2) printf("\n%s\n", printtime(timestr));

      if((dl=fabs(*lnL-lnL0))<e0 && e<=0.02) break;
      if(dl<small_improvement) small_times++;
      else                     small_times=0;
      if((small_times>max_small_times && ntime0<200) || (com.method==2&&ir==1)) {
         if(noisy && com.method!=2) puts("\nToo slow, switching algorithm.");
         status=2;
         break;
      }
      if(noisy && small_times>5) 
         printf("\n%d rounds of small improvement.",small_times);

      e/=2;  if(dl<1) e/=2;
      if(dl<0.5) e=min2(e,1e-3); 
      else if(dl>10) e=max2(e,0.1); 
      e_minbranches=max2(e, 1e-6);
      e=max2(e,1e-6);

      lnL0= *lnL;
      if(fout) {
         fprintf(fout,"%4d %12.5f x ", ir+1,*lnL);
         for(i=0;i<com.np;i++) fprintf(fout,"%9.5f",x[i]);
         FPN(fout);  fflush(fout);
      }
   }
   if (npcom && ir==maxr) status=-1;

   if(npcom && status==2) {
      noisy_minbranches=0;
      com.ntime=ntime0; com.fix_blength=fix_blength0;
      ming2(NULL,lnL,com.plfun,NULL,x,xb, space,e0,com.np);
      FOR(i,tree.nnode) space[i]=-1;
   }
   FOR(i,tree.nnode)
      if(i!=tree.root) x[nodes[i].ibranch]=nodes[i].branch;
   if(noisy>2) printf("\nlnL  = %12.6f\n",- *lnL);

   com.ntime=ntime0; com.fix_blength=fix_blength0;
   *lnL=com.plfun(x,com.np); /* restore things, for e.g. AncestralSeqs */
   free(space_minbranches);

   return (status==-1 ? -1 : 0);
}


/*********************  START: Testing iteration algorithm ******************/

int minB2 (FILE*fout, double *lnL,double x[],double xb[][2],double e0, double space[])
{
/* 
*/
   int ntime0=com.ntime, fix_blength0=com.fix_blength;
   int status=0, i, npcom=com.np-com.ntime;
   double *xcom=x+com.ntime, lnL0= *lnL;
   double (*xbcom)[2]=xb+ntime0;

   i = (3*com.ncode*com.ncode + (com.conPSiteClass) * 4*com.npatt) *sizeof(double);
   if((space_minbranches=(double*)malloc(i))==NULL)  error2("oom minB2");
   if(com.ntime==0 || npcom==0) error2("minB2: should not come here");

   noisy_minbranches=0;
   /* if(*lnL<=0)  *lnL=com.plfun(x,com.np); */
   com.ntime=0; com.fix_blength=2;
#if(CODEML)
   if(com.NSsites==0) com.pomega=xcom+com.nrgene+!com.fix_kappa;
#endif

   ming2(NULL, lnL, minbranches, NULL, xcom, xbcom, space, e0, npcom);


   com.ntime=ntime0; com.fix_blength=fix_blength0;
   FOR(i,tree.nnode)  if(i!=tree.root) x[nodes[i].ibranch]=nodes[i].branch;
   *lnL=com.plfun(x,com.np); /* restore things, for e.g. AncestralSeqs */
   free(space_minbranches);

   return (status==-1 ? -1 : 0);
}

/*********************  END: Testing iteration algorithm ******************/



int updateconP (double x[], int inode)
{
/* update conP for inode.  

   Confusing decision about x[] follows.  Think about redesign.

   (1) Called by PostProbNode for ancestral reconstruction, with com.clock = 0, 
       1, 2: x[] is passed over and com.ntime is used to get xcom in 
       SetPSiteClass()
   (2) Called from minbranches(), with com.clock = 0.  xcom[] is passed 
       over by minbranches and com.ntime=0 is set.  So SetPSiteClass()
       can still get the correct substitution parameters.  
       Also look at ConditionalPNode().
  
   Note that com.nodeScaleF and nodes[].conP are shifted if(com.conPSiteClass).
*/
   int ig,i,ir;

   if(com.conPSiteClass==0)
      FOR(ig,com.ngene) {
         if(com.Mgene>1 || com.nalpha>1)
            SetPGene(ig,com.Mgene>1,com.Mgene>1,com.nalpha>1,x);
         /* x[] needed by local clock models and if(com.aaDist==AAClasses).
            This is called from PostProbNode  */
         ConditionalPNode(inode, ig, x);
      }
   else {  /* site-class models */
      FOR(ir,com.ncatG) {
#ifdef CODEML
         IClass=ir;
#endif
         if(ir) {
            if(com.NnodeScale) com.nodeScaleF+=com.NnodeScale*com.npatt;
            for(i=com.ns;i<tree.nnode;i++)
               nodes[i].conP+=(tree.nnode-com.ns)*com.ncode*com.npatt;
         }
         SetPSiteClass(ir, x);
         FOR(ig,com.ngene) {
            if(com.Mgene>1 || com.nalpha>1)
               SetPGene(ig,com.Mgene>1,com.Mgene>1,com.nalpha>1,x);
            if(com.nalpha>1) SetPSiteClass(ir, x);
            ConditionalPNode(inode,ig, x);
         }
      }

      /* shift positions */
      com.nodeScaleF-=(com.ncatG-1)*com.NnodeScale*com.npatt;
      for(i=com.ns;i<tree.nnode;i++)
         nodes[i].conP-=(com.ncatG-1)*(tree.nnode-com.ns)*com.ncode*com.npatt;
   }
   return(0);
}


double minbranches (double x[], int np)
{
/* Z. Yang, November 1999.
   optimizing one branch at a time
   
   for each branch a..b, reroot the tree at b, and 
   then calculate conditional probability for node a.
   For each branch, this routine determines the Newton search direction 
   p=-dl/dll.  It then halves the steplength to make sure -lnL is decreased.
   When the Newton solution is correct, this strategy will waste one 
   extra call to lfunt.  It does not seem possible to remove calculation of 
   l (lnL) in lfuntddl().
   lfun or lfundG and thus SetParameters are called once beforehand to set up 
   globals like com.pomega.
   This works with NSsites and NSbranch models.
   
   com.oldconP[] marks nodes that need to be updated when the tree is rerooted.  
   The array is declared in baseml and codeml and used in the following 
   routines: ReRootTree, minbranches, and ConditionalPNode.

   Note: At the end of the routine, nodes[].conP are not updated.
*/
   int ib,oldroot=tree.root, a,b;
   int icycle, maxcycle=1000, icycleb,ncycleb=10, i;
   double lnL, lnL0=0, l0,l,dl,ddl=-1, t,t0,t00, p,step=1, small=1e-20,y;
   double tb[2]={1e-8,50}, e=e_minbranches, *space=space_minbranches;
   double *xcom=x+com.ntime;  /* this is incorrect as com.ntime=0 */
   double smallvarb=0.25/com.ls*(1-0.25/com.ls)/com.ls;

   if(com.ntime) error2("ntime should be 0 in minbranches");
   lnL0=l0=l=lnL=com.plfun(xcom,-1);

   if(noisy_minbranches>2) printf("\tlnL0 =    %14.6f\n",-lnL0);

   FOR(icycle,maxcycle) {
      for(ib=0; ib<tree.nbranch; ib++) {
         t=t0=t00=nodes[tree.branches[ib][1]].branch; 
         l0=l;
         a=tree.branches[ib][0]; b=tree.branches[ib][1];
         FOR(i,tree.nnode) com.oldconP[i]=1;
         ReRootTree(b);
         updateconP(x,a);

         for(icycleb=0; icycleb<ncycleb; icycleb++) {  /* iterating a branch */
            if(!com.conPSiteClass)
               lfuntdd(t,a,b,xcom, &y,&dl,&ddl,space);
            else
               lfuntdd_SiteClass(t,a,b,xcom, &y,&dl,&ddl,space);

            if(fabs(y-l)>1e-3 && noisy_minbranches>2)
               printf("\nWarning rounding error? b=%d cycle=%d lnL=%12.7f != %12.7f\n",ib,icycleb,l,y);
            p=-dl/fabs(ddl);
            /* p=-dl/ddl; newton direction */
            if (fabs(p)<small) step=0;
            else if(p<0)       step=min2(1,(tb[0]-t0)/p);
            else               step=min2(1,(tb[1]-t0)/p);

            if(icycle==0 && step!=1 && step!=0) step*=0.99; /* avoid border */
            for (i=0; step>small; i++,step/=4) {
               t=t0+step*p;
               if(!com.conPSiteClass) lfunt(t, a,b,xcom, &l, space);
               else                   lfunt_SiteClass(t, a,b,xcom, &l,space);
               if(l<l0) break;
            }
            if(step<=small) { t=t0; l=l0; break; }
            if(fabs(t-t0)<e*fabs(1+t) && fabs(l-l0)<e) break;
            t0=t; l0=l;
         }
         nodes[a].branch=t;

         varb_minbranches[b] = ((t>1e-6&&ddl>1e-100) ? 1/ddl : smallvarb);

      }   /* for (ib) */
      lnL=l;
      if(noisy_minbranches>2) printf("\tCycle %2d: %14.6f\n",icycle+1, -l);
      if(fabs(lnL-lnL0)<e) break;
      lnL0= lnL;
   }  /* for (icycle) */
   ReRootTree(oldroot);  /* did not update conP */
   FOR(i,tree.nnode) com.oldconP[i]=0;
   return(lnL);
}



int lfunt(double t, int a,int b,double xcom[],double *l, double space[])
{
/* See notes for lfunt_dd and minbranches
*/
   int i,j,k, h,ig,n=com.ncode, nroot=n;
   int n1=(com.cleandata&&b<com.ns?1:n);
   double expt,uexpt=0,multiply;
   double *P=space, piqi,pqj, fh;
   double *pkappa;

#if (CODEML)
   pkappa=(com.hkyREV||com.codonf>=FMutSel3x4?xcom+com.nrgene:&com.kappa);
   if (com.seqtype==CODONseq && com.model) {
      if(com.model==2 && com.nOmega<=5) {
         U=_UU[(int)nodes[a].label]; 
         V=_VV[(int)nodes[a].label]; 
         Root=_Root[(int)nodes[a].label]; 
      }
      else {
         EigenQc(0,-1,NULL,NULL,NULL,Root,U,V, pkappa, nodes[a].omega, PMat);
      }
   }
#endif

#if (BASEML)
   if (com.nhomo==2)
      EigenTN93(com.model,nodes[a].kappa,1,com.pi,&nR,Root,Cijk);
   nroot=nR;
#endif

   *l = 0;
   for (ig=0; ig<com.ngene; ig++) {
      if(com.Mgene>1) SetPGene(ig,1,1,0,xcom); /* com.ntime=0 */
      FOR(i,n*n) P[i]=0;

      for(k=0,expt=1; k<nroot; k++) {
         multiply=com.rgene[ig]*Root[k];
         if(k) expt=exp(t*multiply);

#if (CODEML)  /* uses U & V */
         FOR(i,n) for(j=0,uexpt=U[i*n+k]*expt; j<n; j++)
            P[i*n+j]+=uexpt*V[k*n+j];
#elif (BASEML) /* uses Cijk */
         FOR(i,n) FOR(j,n)
            P[i*n+j]+=Cijk[i*n*nroot+j*nroot+k]*expt;
#endif
      }

      for (h=com.posG[ig]; h<com.posG[ig+1]; h++) {
         for(i=0,fh=0; i<n1; i++) {
            if(n1==1) piqi=com.pi[i=com.z[b][h]];
            else      piqi=com.pi[i]*nodes[b].conP[h*n+i];

            for(j=0,pqj=0; j<n; j++)
               pqj+=P[i*n+j]*nodes[a].conP[h*n+j];
            fh+=piqi*pqj;
         }
         if(noisy && fh<1e-250) printf("a bit too small: fh[%d] = %10.6e\n",h,fh);
         if(fh<0) fh=-500;
         else     fh=log(fh);

         *l-=fh*com.fpatt[h];
         FOR(i,com.NnodeScale) *l-=com.nodeScaleF[i*com.npatt+h]*com.fpatt[h];
      }
   }
   return(0);
}


int lfuntdd(double t, int a,int b,double xcom[],double *l,double*dl,double*ddl,
    double space[])
{
/* Calculates lnL for branch length t for branch b->a.
   See notes in minbranches().
   Conditional probability updated correctly already.

   i for b, j for a?
*/
   int i,j,k, h,ig,n=com.ncode, nroot=n;
   int n1=(com.cleandata&&b<com.ns?1:n);
   double expt,uexpt=0,multiply;
   double *P=space, *dP=P+n*n,*ddP=dP+n*n, piqi,pqj,dpqj,ddpqj, fh,dfh,ddfh;
   double *pkappa;

#if(CODEML)
   pkappa=(com.hkyREV||com.codonf>=FMutSel3x4?xcom+com.nrgene:&com.kappa);
   if (com.seqtype==CODONseq && com.model) {
      if(com.model==2 && com.nOmega<=5) {
         U=_UU[(int)nodes[a].label]; 
         V=_VV[(int)nodes[a].label]; 
         Root=_Root[(int)nodes[a].label]; 
      }
      else {
         EigenQc(0,-1,NULL,NULL,NULL,Root,U,V, pkappa, nodes[a].omega,PMat);
      }
   }
#endif

#if(BASEML)
   if (com.nhomo==2)
      EigenTN93(com.model,nodes[a].kappa,1,com.pi,&nR,Root,Cijk);
   nroot=nR;
#endif
   *l=*dl=*ddl=0;
   for (ig=0; ig<com.ngene; ig++) {
      if(com.Mgene>1) SetPGene(ig,1,1,0,xcom);  /* com.ntime=0 */
      FOR(i,n*n) P[i]=dP[i]=ddP[i]=0;

      for(k=0,expt=1; k<nroot; k++) {
         multiply=com.rgene[ig]*Root[k];
         if(k) expt=exp(t*multiply);

#if (CODEML)  /* uses U & V */
         FOR(i,n) for(j=0,uexpt=U[i*n+k]*expt; j<n; j++) {
            P[i*n+j]+=uexpt*V[k*n+j];
            if(k) {
               dP[i*n+j]+=uexpt*V[k*n+j]*multiply;
               ddP[i*n+j]+=uexpt*V[k*n+j]*multiply*multiply;
            }
         }
#elif (BASEML) /* uses Cijk */
         FOR(i,n) FOR(j,n) {
            P[i*n+j]+=Cijk[i*n*nroot+j*nroot+k]*expt;
            if(k) {
               dP[i*n+j]+=Cijk[i*n*nroot+j*nroot+k]*expt*multiply;
               ddP[i*n+j]+=Cijk[i*n*nroot+j*nroot+k]*expt*multiply*multiply;
            }
         }
#endif
      }
      for (h=com.posG[ig]; h<com.posG[ig+1]; h++) {
         for(i=0,fh=dfh=ddfh=0; i<n1; i++) {
            if(n1==1) piqi=com.pi[i=com.z[b][h]];
            else      piqi=com.pi[i]*nodes[b].conP[h*n+i];

            for(j=0,pqj=dpqj=ddpqj=0; j<n; j++) {
               pqj+=P[i*n+j]*nodes[a].conP[h*n+j];
               dpqj+=dP[i*n+j]*nodes[a].conP[h*n+j];
               ddpqj+=ddP[i*n+j]*nodes[a].conP[h*n+j];
            }
            fh+=piqi*pqj;
            dfh+=piqi*dpqj;
            ddfh+=piqi*ddpqj;
         }
         if(fh<1e-250) {
            printf("too small: fh[%d] = %10.6e\n",h,fh);
            OutaTreeN(F0,0,1);
         }
         *l -= log(fh)*com.fpatt[h];
         FOR(i,com.NnodeScale) *l-=com.nodeScaleF[i*com.npatt+h]*com.fpatt[h];
         *dl -= dfh/fh*com.fpatt[h];
         *ddl-= (-dfh*dfh+fh*ddfh)/(fh*fh) * com.fpatt[h];
      }
   }  /* for(ig) */
   return(0);
}


int lfunt_SiteClass(double t, int a,int b,double xcom[],double *l,double space[])
{
/* see notes in lfuntdd_SiteClass
   For branch&site models, look at the notes in GetPMatBranch()
*/
   int i,j,k, h,ig,ir,it, n=com.ncode, nroot=n;
   int n1=(com.cleandata&&b<com.ns?1:n);  /* one state for a tip? */
   double y,expt,uexpt=0,multiply, piqi,pqj;
   double *P=space, *fh=P+n*n;
   double *Sh=fh+com.npatt;  /* scale factor for each site pattern*/
   double *pK=com.fhK;  /* proportion for each site class after scaling */
   double smallw=1e-12; 

#if (BASEML)
   if (com.nhomo==2)
      EigenTN93(com.model,nodes[a].kappa,1,com.pi,&nR,Root,Cijk);
   nroot=nR;
#endif

   if(com.NnodeScale==0) 
      FOR(ir,com.ncatG) FOR(h,com.npatt)  pK[ir*com.npatt+h]=com.freqK[ir];
   else {
      FOR(h,com.npatt) {
         for(ir=0,it=0; ir<com.ncatG; ir++) {
            for(k=0,y=0; k<com.NnodeScale; k++)
               y+=com.nodeScaleF[ir*com.NnodeScale*com.npatt + k*com.npatt+h];
            if((pK[ir*com.npatt+h]=y)>pK[it*com.npatt+h]) it=ir;
         }
         Sh[h]=pK[it*com.npatt+h];
         FOR(ir,com.ncatG)
            pK[ir*com.npatt+h]=com.freqK[ir]*exp(pK[ir*com.npatt+h]-Sh[h]);
      }
   }

   FOR(h,com.npatt) fh[h]=0;
   FOR(ir,com.ncatG) {
      SetPSiteClass(ir, xcom);  /* com.ntime=0 */

#if CODEML  /* branch b->a */
      /* branch&site models */
      if(com.seqtype==CODONseq && com.NSsites && com.model)
         Set_UVR_BranchSite (ir, (int)nodes[a].label);
#endif

      if(ir) {
         for(i=com.ns;i<tree.nnode;i++)
            nodes[i].conP+=(tree.nnode-com.ns)*n*com.npatt;
      }
      for (ig=0; ig<com.ngene; ig++) {
         if(com.Mgene>1 || com.nalpha>1)
            SetPGene(ig,com.Mgene>1,com.Mgene>1,com.nalpha>1,xcom);  /* com.ntime=0 */
         if(com.nalpha>1) SetPSiteClass(ir, xcom);    /* com.ntime=0 */

         FOR(i,n*n) P[i]=0;
         for(k=0,expt=1; k<nroot; k++) {
            multiply=com.rgene[ig]*Root[k]*_rateSite;
            if(k) expt=exp(t*multiply);

#if (CODEML)  /* uses U & V */
            FOR(i,n) for(j=0,uexpt=U[i*n+k]*expt; j<n; j++)
               P[i*n+j]+=uexpt*V[k*n+j];
#elif (BASEML) /* uses Cijk */
            FOR(i,n) FOR(j,n)
               P[i*n+j]+=Cijk[i*n*nroot+j*nroot+k]*expt;
#endif
         }  /* for (k), look through eigenroots */
         for (h=com.posG[ig]; h<com.posG[ig+1]; h++) {
            for(i=0; i<n1; i++) {
               if(n1==1) piqi=pK[ir*com.npatt+h]*com.pi[i=com.z[b][h]];
               else      piqi=pK[ir*com.npatt+h]*com.pi[i]*nodes[b].conP[h*n+i];

               for(j=0,pqj=0; j<n; j++)
                  pqj+=P[i*n+j]*nodes[a].conP[h*n+j];
               fh[h]+=piqi*pqj;
            }
         }  /* for (h) */
      }     /* for (ig) */
   }        /* for(ir) */

   for(i=com.ns;i<tree.nnode;i++)  /* shift position */
      nodes[i].conP-=(com.ncatG-1)*(tree.nnode-com.ns)*n*com.npatt;
   for(h=0,*l=0; h<com.npatt; h++) {
      if(fh[h]<1e-250) 
         printf("small (lfunt_SiteClass): fh[%d] = %10.6e\n",h,fh[h]);

      *l-=log(fh[h])*com.fpatt[h];
      if(com.NnodeScale) *l -= Sh[h]*com.fpatt[h];
   }
   return(0);
}


int lfuntdd_SiteClass(double t, int a,int b,double xcom[],
    double *l,double*dl,double*ddl,double space[])
{
/* dt and ddt for site-class models, modified from lfuntdd()
   nodes[].conP (and com.nodeScaleF if scaling is used) is shifted for ir, 
   and moved back to the rootal place at the end of the routine.

   At the start of this routine, nodes[].conP has the conditional probabilties 
   for each node, each site pattern, for each site class (ir).  
   Scaling: When scaling is used, scale factors 
   com.nodeScaleF[ir*com.NnodeScale*com.npatt + k*com.npatt+h] for all nodes 
   are collected into Sh[h], after adjusting for rate classes, since the 
   sum is taken over ir.  Sh[h] and pK[ir*com.npatt+h] together store the 
   scale factors and proportions for site classes.  com.freqK[ir] is not 
   used in this routine beyond this point.
   if(com.Malpha), com.freqK[]=1/com.ncatG and does not change with ig, 
   and so the collection of Sh for sites at the start of the routine is o.k.

   The space for com.fhK[] is used.
   space[2*ncode*ncode + 4*npatt]:
     dP[ncode*ncode],ddP[ncode*ncode],fh[npatt],dfh[npatt],ddfh[npatt],Sh[npatt]
     pK[ncatG*npatt]=com.fhK[]
*/
   int i,j,k, h,ig,ir,it, n=com.ncode, nroot=n;
   int n1=(com.cleandata&&b<com.ns?1:n);  /* one state for a tip? */
   double y,expt,uexpt=0,multiply, piqi,pqj,dpqj,ddpqj;
   double *P=PMat, *dP=space,*ddP=dP+n*n;
   double *fh=ddP+n*n, *dfh=fh+com.npatt, *ddfh=dfh+com.npatt;
   double *Sh=ddfh+com.npatt;  /* scale factor for each site pattern */
   double *pK=com.fhK;  /* proportion for each site class after scaling */
   double smallw=1e-12; 

#if (BASEML)
   if (com.nhomo==2)
      EigenTN93(com.model,nodes[a].kappa,1,com.pi,&nR,Root,Cijk);
   nroot=nR;
#endif
   k=(2*n*n+4*com.npatt)*(int)sizeof(double);
   if(com.sspace<k) {  /* this assumes that space is com.space */
      printf("\n%d bytes in space, %d bytes needed", com.sspace, k);
      error2("lfuntdd_SiteClass: not enough memory is allocated.\nLet me know");
   }
   if(com.NnodeScale==0)
      FOR(ir,com.ncatG) FOR(h,com.npatt)  pK[ir*com.npatt+h]=com.freqK[ir];
   else {
      FOR(h,com.npatt) {
         for(ir=0,it=0; ir<com.ncatG; ir++) {
            for(k=0,y=0; k<com.NnodeScale; k++)
               y+=com.nodeScaleF[ir*com.NnodeScale*com.npatt + k*com.npatt+h];
            if((pK[ir*com.npatt+h]=y)>pK[it*com.npatt+h]) it=ir;
         }
         Sh[h]=pK[it*com.npatt+h];
         FOR(ir,com.ncatG)
            pK[ir*com.npatt+h]=com.freqK[ir]*exp(pK[ir*com.npatt+h]-Sh[h]);
      }
   }

   FOR(h,com.npatt) fh[h]=dfh[h]=ddfh[h]=0;
   FOR(ir,com.ncatG) {
      SetPSiteClass(ir, xcom);   /* com.ntime=0 */

#if CODEML  /* branch b->a */
      /* branch&site models */
      if(com.seqtype==CODONseq && com.NSsites && com.model)
         Set_UVR_BranchSite (ir, (int)nodes[a].label);
#endif

      if(ir) {
         for(i=com.ns;i<tree.nnode;i++)
            nodes[i].conP+=(tree.nnode-com.ns)*n*com.npatt;
      }
      for (ig=0; ig<com.ngene; ig++) {
         if(com.Mgene>1 || com.nalpha>1)
            SetPGene(ig,com.Mgene>1,com.Mgene>1,com.nalpha>1,xcom);   /* com.ntime=0 */
         if(com.nalpha>1) SetPSiteClass(ir, xcom);   /* com.ntime=0 */

         FOR(i,n*n) P[i]=dP[i]=ddP[i]=0;
         for(k=0,expt=1; k<nroot; k++) {
            multiply=com.rgene[ig]*Root[k]*_rateSite;
            if(k) expt=exp(t*multiply);

#if (CODEML)  /* uses U & V */
            FOR(i,n) for(j=0,uexpt=U[i*n+k]*expt; j<n; j++) {
               P[i*n+j]+=uexpt*V[k*n+j];
               if(k) {
                  dP[i*n+j]+=uexpt*V[k*n+j]*multiply;
                  ddP[i*n+j]+=uexpt*V[k*n+j]*multiply*multiply;
               }
            }
#elif (BASEML) /* uses Cijk */
            FOR(i,n) FOR(j,n) {
               P[i*n+j]+=Cijk[i*n*nroot+j*nroot+k]*expt;
               if(k) {
                  dP[i*n+j]+=Cijk[i*n*nroot+j*nroot+k]*expt*multiply;
                  ddP[i*n+j]+=Cijk[i*n*nroot+j*nroot+k]*expt*multiply*multiply;
               }
            }
#endif
         }  /* for (k), loop through eigenroots */

         for (h=com.posG[ig]; h<com.posG[ig+1]; h++) {
            for(i=0; i<n1; i++) {
               if(n1==1) piqi=pK[ir*com.npatt+h]*com.pi[i=com.z[b][h]];
               else      piqi=pK[ir*com.npatt+h]*com.pi[i]*nodes[b].conP[h*n+i];

               for(j=0,pqj=dpqj=ddpqj=0; j<n; j++) {
                  pqj+=P[i*n+j]*nodes[a].conP[h*n+j];
                  dpqj+=dP[i*n+j]*nodes[a].conP[h*n+j];
                  ddpqj+=ddP[i*n+j]*nodes[a].conP[h*n+j];
               }
               fh[h]+=piqi*pqj;
               dfh[h]+=piqi*dpqj;
               ddfh[h]+=piqi*ddpqj;
            }
         }  /* for (h) */
      }     /* for (ig) */
   }        /* for(ir) */

   for(i=com.ns;i<tree.nnode;i++)
      nodes[i].conP-=(com.ncatG-1)*(tree.nnode-com.ns)*n*com.npatt;
   for(h=0,*l=*dl=*ddl=0; h<com.npatt; h++) {
      if(fh[h]<1e-250) 
         printf("small fh[%d] = %10.6e\n",h,fh[h]);

      *l-=log(fh[h])*com.fpatt[h];
      if(com.NnodeScale) *l-=Sh[h]*com.fpatt[h];
      *dl-=dfh[h]/fh[h] * com.fpatt[h];
      *ddl-=(-dfh[h]*dfh[h]+fh[h]*ddfh[h])/(fh[h]*fh[h]) * com.fpatt[h];
   }

   return(0);
}

#endif


#endif         /* #ifdef LFUNCTIONS */

#ifdef BIRTHDEATH

void BranchLengthBD(int rooted, double birth, double death, double sample, 
     double mut)
{
/* Generate random branch lengths (nodes[].branch) using the birth and
   death process with species sampling, or the Yule (coalescent?) process
   if sample=0, when only parameter mut is used.
   Note: older interior nodes have larger node numbers, so root is at
   node com.ns*2-2 with time t[ns-2], while the youngest node is at 
   node com.ns with time t[0].  When unrooted=0, the root is removed with
   branch lengths adjusted.
   This works with the tree generated from RandomLHistory().
*/
   int i,j, it, imin,fixt0=1;
   double la=birth, mu=death, rho=sample, tmin, r, t[NS-1];
   double phi, eml, y;

   if (sample==0)  /* coalescent model */
      for (i=com.ns,y=0; i>1; i--) 
          nodes[com.ns*2-i].age=y += -log(rndu())/(i*(i-1.)/2.)*mut/2;
   else  {         /* BD with sampling */
      if (fixt0) t[com.ns-2]=1;
      if (fabs(la-mu)>1e-6) {
         eml=exp(mu-la);  phi=(rho*la*(eml-1)+(mu-la)*eml)/(eml-1);
         for (i=0; i<com.ns-1-(fixt0); i++) {
           r=rndu(); t[i]=log((phi-r*rho*la)/(phi-r*rho*la+r*(la-mu)))/(mu-la);
       }
      }
      else  
         for (i=0; i<com.ns-1-(fixt0); i++) 
            { r=rndu();  t[i]=r/(1+la*rho*(1-r)); }
      /* bubble sort */
      for (i=0; i<com.ns-1-1; i++) {
         for (j=i+1,tmin=t[i],imin=i; j<com.ns-1; j++) 
            if (tmin>t[j]) { tmin=t[j]; imin=j; }
         t[imin]=t[i];  t[i]=tmin;
      }
      for (i=com.ns; i>1; i--) nodes[com.ns*2-i].age=t[com.ns-i]*mut;
   }
   FOR (i,com.ns) nodes[i].age=0;
   for (i=0; i<tree.nnode; i++) 
      if (i!=tree.root) 
         nodes[i].branch=nodes[nodes[i].father].age-nodes[i].age;
   if (!rooted) {
      it=nodes[tree.root].sons[2];
      nodes[it].branch =
      2*nodes[2*com.ns-2].age-nodes[tree.root].age-nodes[it].age;
   }
}

#endif


#ifdef NODESTRUCTURE
#ifdef EVOLVER

int RandomLHistory (int rooted, double space[])
{
/* random coalescence tree, with each labeled history having equal probability.
   interior nodes are numbered ns, ns+1, ..., 2*ns-1-!rooted
*/
   int ns=com.ns, i, j, it=0, *nodea=(int*)space;
   double t;

   for (i=0; i<2*ns-1-!rooted; i++) ClearNode(i);

   for (i=0; i<ns; i++) nodea[i]=i;
   for (i=ns,t=0; i>(1+!rooted); i--) {
      nodes[it=2*ns-i].nson=2;
      j=(int)(i*rndu()); 
      nodes[nodea[j]].father=it; nodes[it].sons[0]=nodea[j];
      nodea[j]=nodea[i-1];
      j=(int)((i-1)*rndu()); 
      nodes[nodea[j]].father=it; nodes[it].sons[1]=nodea[j];
      nodea[j]=it;
      if (!rooted && i==3) {
         nodes[it].nson++; 
         nodes[nodea[1-j]].father=it; nodes[it].sons[2]=nodea[1-j];
      }
   }
   tree.root=it;  tree.nnode=ns*2-1-!rooted;
   NodeToBranch();
   return (0);
}

#endif

#endif  /* NODESTRUCTURE */


int PatternWeightSimple (int CollapsJC, double space[])
{
/* Counts site patterns, changes .z[], .fpatt[], .npatt, etc.
   This is modified from PatternWeight(), and does not deal with
   multiple genes.
*/   
   int  h, ht, b,j,k, same=0;
   char *zt[NS], zh[NS];

if(!com.cleandata) 
   puts("\nPatternWeightSimple: missing data not dealt with yet");

   FOR (j,com.ns) zt[j]=(char*)space+j*com.ls;
   FOR (h,com.ls) com.fpatt[h]=0; 

   for (h=0,com.npatt=0; h<com.ls; h++) {
      if (CollapsJC) {
         zh[0]=(char)(b=0);
         for (j=1; j<com.ns; j++) {
            for (k=0; k<j; k++) if (com.z[j][h]==com.z[k][h]) break;
            zh[j]=(char)(k<j?zh[k]:++b);
         }
      }
      else 
         for (j=0; j<com.ns; j++) zh[j]=com.z[j][h];
      for (ht=0,same=0; ht<com.npatt; ht++) {
         for (j=0,same=1; j<com.ns; j++)
          if (zt[j][ht]!=zh[j]) { same=0; break; }
         if (same)  break; 
      }
      if (same)   com.fpatt[ht]++;
      else {
         FOR (j, com.ns) zt[j][com.npatt]=zh[j];
         com.fpatt[com.npatt++]=1;
      }
   }
   FOR (j,com.ns) {
      memcpy (com.z[j], zt[j], com.npatt);
      com.z[j][com.npatt]=0;
   }
   return (0);
}






/* routines for dating analysis of heterogeneous data */
#if (defined BASEML || defined CODEML || defined MCMCTREE)


extern double gamma_pexp[];

int ReadTreeSeqs (FILE*fout)
{
/* This reads the combined species tree, the fossil calibration information, 
   and sequence data at each locus.  sptree.nodes[].tfossil[] has tL, tU for 
   bounds or alpha and beta for the gamma prior.  
   This also constructs the gene tree at each locus, by pruning the master 
   species tree..
*/
   FILE *fseq, *ftree;
   int i,j, locus, lspname=LSPNAME, clean0=com.cleandata;
   double *pe=gamma_pexp, modetiles[3]; /* mode, 2.5%, 97.5% */

   ftree=gfopen(com.treef,"r");
  
   /* read master species tree and process fossil calibration info */
   fscanf(ftree, "%d%d", &sptree.nspecies, &i);
   com.ns=sptree.nspecies;
   /* to read master species names into sptree.nodes[].name */
   if(noisy) puts("Reading master tree.");
   FOR(j,sptree.nspecies) com.spname[j]=sptree.nodes[j].name;
   nodes=nodes_t;
   ReadaTreeN(ftree,&i,&i,1,1);
   if(com.clock==5 || com.clock==6)
      FOR(i,tree.nnode) nodes[i].branch=nodes[i].label=0;
   OutaTreeN(F0,0,0); FPN(F0);
   OutaTreeN(F0,1,0); FPN(F0);
   /* copy master tree into sptree */
   if(tree.nnode!=2*com.ns-1) 
      error2("check and think about multificating trees.");
   sptree.nnode=tree.nnode;  sptree.nbranch=tree.nbranch; 
   sptree.root=tree.root;    sptree.nfossil=0;
   for(i=0; i<sptree.nspecies*2-1; i++) {
      sptree.nodes[i].father=nodes[i].father;
      sptree.nodes[i].nson=nodes[i].nson;
      if(nodes[i].nson!=0 && nodes[i].nson!=2) 
         error2("master tree has to be binary.");
      for(j=0;j<sptree.nodes[i].nson;j++) sptree.nodes[i].sons[j]=nodes[i].sons[j];

      /* collect fossil information */
      sptree.nodes[i].fossil=nodes[i].fossil;
      sptree.nodes[i].age=nodes[i].age;
      sptree.nodes[i].tfossil[0]=nodes[i].branch; /* ">": Lower bound */
      sptree.nodes[i].tfossil[1]=nodes[i].label;  /* "<": Upper bound */

      if(nodes[i].branch && nodes[i].label) {
         modetiles[0]=nodes[i].age;   /* mode */
         modetiles[1]=nodes[i].branch;    /* ">": Lower bound */
         modetiles[2]=nodes[i].label;     /* "<": Upper bound */
         if(modetiles[0]==0)
            sptree.nodes[i].fossil = BOUND_F;
         else {
            getab_gamma(&sptree.nodes[i].tfossil[0], &sptree.nodes[i].tfossil[1], modetiles);
            printf("\na = %8.6f  b = %8.6f  CDFs (%8.6f, %8.6f) %8.6f\n", sptree.nodes[i].tfossil[0],sptree.nodes[i].tfossil[1], pe[0],pe[1], pe[1]-pe[0]);
            sptree.nodes[i].fossil = GAMMA_F; 
         }
         sptree.nfossil++;
      }
      else if(nodes[i].branch) 
         { sptree.nodes[i].fossil = LOWER_F; sptree.nfossil++; }
      else if(nodes[i].label) 
         { sptree.nodes[i].fossil = UPPER_F; sptree.nfossil++; }

      nodes[i].branch=nodes[i].label=0;
   }

   /* read sequences at each locus, construct gene tree by pruning sptree */
   data.ngene=com.ndata;  com.ndata=1;
   fseq=gfopen(com.seqf,"r");
   if((gnodes=(struct TREEN**)malloc(sizeof(struct TREEN*)*data.ngene)) == NULL) 
      error2("oom");

   printf("\nReading sequence data..  %d loci\n", data.ngene);
   for(locus=0; locus<data.ngene; locus++) {
      fprintf(fout, "\n\n*** Locus %d ***\n", locus+1);
      printf("\n\n*** Locus %d ***\n", locus+1);

      com.cleandata=(char)clean0;
      FOR(j,sptree.nspecies) com.spname[j]=NULL; /* points to nowhere */
#if (defined CODEML)
      if(com.seqtype==1) {
         com.icode=data.icode[locus];
         setmark_61_64 ();
      }
#endif
      ReadSeq (NULL, fseq);                      /* allocates com.spname[] */
#if (defined CODEML)
      if(com.seqtype==1) {
         if(com.sspace<max2(com.ngene+1,com.ns)*(64+12+4)*(int)sizeof(double)) {
            com.sspace=max2(com.ngene+1,com.ns)*(64+12+4)*sizeof(double);
            if((com.space=(double*)realloc(com.space,com.sspace))==NULL)
               error2("oom space for #c");
         }
         InitializeCodon(fout,com.space);
      }
#endif
      if(com.seqtype==0 || com.seqtype==2)
         Initialize(fout);
      if((com.seqtype==0 || com.seqtype==2) && com.model==0) 
         PatternJC69like(fout);

      xtoy(com.pi, data.pi[locus], com.ncode);

      data.cleandata[locus] = (char)com.cleandata;
      data.ns[locus]=com.ns;  
      data.ls[locus]=com.ls;  
      data.npatt[locus]=com.npatt;
      data.fpatt[locus]=com.fpatt; com.fpatt=NULL;
      for(i=0; i<com.ns; i++) { data.z[locus][i]=com.z[i]; com.z[i]=NULL; }

      printf("%3d patterns, %s\n", com.npatt,(com.cleandata?"clean":"messy"));
      GetGtree(locus);      /* free com.spname[] */
   }
   for(i=0,com.cleandata=1; i<data.ngene; i++) 
      if(!data.cleandata[i]) com.cleandata=0;

   fclose(ftree); fclose(fseq);
   return(0);
}




int GetGtree (int locus)
{
/* construct the gene tree at locus by pruning tips in the master species 
   tree.  com.spname[] have names of species at the current locus and 
   the routine use them to compare with sptree.nodes[].name to decide which 
   species to keep for the locus.  See GetSubTreeN() for more info.
*/
   int ns=data.ns[locus], i,j, ipop[NS], keep[NS], newnodeNO[2*NS-1];

   FOR(j,sptree.nspecies) keep[j]=0;
   FOR(i,ns) {
      FOR(j,sptree.nspecies) 
         if(!strcmp(com.spname[i], sptree.nodes[j].name)) break;
      if(j==sptree.nspecies) {
         printf("species %s not found in master tree\n", com.spname[i]);
         exit(-1);
      }
      keep[j]=i+1; ipop[i]=j;
      free(com.spname[i]);
   }
   /* copy master species tree and then prune it. */
   copySptree();
   GetSubTreeN(keep, newnodeNO);
   com.ns=ns;

   FOR(i,sptree.nnode)  
      if(newnodeNO[i]!=-1) nodes[newnodeNO[i]].ipop=i;
   /* if(debug) */ 
      printGtree(0);

   gnodes[locus]=(struct TREEN*)malloc((ns*2-1)*sizeof(struct TREEN));
   if(gnodes[locus]==NULL) error2("oom gtree");
   memcpy(gnodes[locus], nodes, (ns*2-1)*sizeof(struct TREEN));
   data.root[locus]=tree.root;

   return(0);
}


void printGtree (int printBlength)
{
   int i,j;

   FOR(i,com.ns) com.spname[i]=sptree.nodes[nodes[i].ipop].name;
   FOR (i,tree.nnode) if(i!=tree.root) 
      nodes[i].branch=nodes[nodes[i].father].age-nodes[i].age;
   printf("\nns = %d  nnode = %d", com.ns, tree.nnode);
   printf("\n%7s%7s %8s %7s%7s","father","node","(ipop)","nson:","sons");
   FOR (i, tree.nnode) {
      printf ("\n%7d%7d   (%2d) %7d  ",
         nodes[i].father, i, nodes[i].ipop, nodes[i].nson);
      FOR(j, nodes[i].nson) printf (" %2d", nodes[i].sons[j]);
   }
   FPN(F0); OutaTreeN(F0,0,0); FPN(F0); OutaTreeN(F0,1,0); FPN(F0); 
   if(printBlength) { OutaTreeN(F0,1,1); FPN(F0); }
}


void copySptree (void)
{
/* This copies sptree into nodes = nodes_t, for printing or editing
*/
   int i,j;

   nodes=nodes_t;
   com.ns=sptree.nspecies;   tree.root=sptree.root;
   tree.nnode=sptree.nnode;  tree.nbranch=sptree.nbranch; 
   for(i=0; i<sptree.nnode; i++) {
      if(i<com.ns) com.spname[i]=sptree.nodes[i].name;
      nodes[i].father=sptree.nodes[i].father;
      nodes[i].nson=sptree.nodes[i].nson;
      for(j=0;j<nodes[i].nson;j++) nodes[i].sons[j]=sptree.nodes[i].sons[j];
      nodes[i].fossil=sptree.nodes[i].fossil;
      nodes[i].age=sptree.nodes[i].age;
      if(i!=tree.root) 
         nodes[i].branch=sptree.nodes[nodes[i].father].age-sptree.nodes[i].age;
   }
}

void printSptree (void)
{
   int i;

   printf("\n************\nSpecies tree\nns = %d  nnode = %d", sptree.nspecies, sptree.nnode);
   printf("\n%7s%7s  %-8s %12s %12s%16s\n","father","node","name","time","fossil","sons");
   FOR (i, sptree.nnode) {
      printf("%7d%7d  %-14s %9.5f ", 
         sptree.nodes[i].father, i, sptree.nodes[i].name, sptree.nodes[i].age);
      printf("  %2s %6.2f %6.2f", 
         fossils[(int)sptree.nodes[i].fossil], sptree.nodes[i].tfossil[0], sptree.nodes[i].tfossil[1]);
      if(sptree.nodes[i].nson)
         printf("  (%2d %2d)", sptree.nodes[i].sons[0], sptree.nodes[i].sons[1]);
      FPN(F0);
   }
   copySptree();
   FPN(F0); OutaTreeN(F0,0,0); FPN(F0); OutaTreeN(F0,1,0);  FPN(F0); 
   OutaTreeN(F0,1,1); FPN(F0);
}


#endif

#if (defined BASEML || defined CODEML)

#if (defined CODEML)

int GetMemPUVR(int nc, int nUVR)
{
/* this gets mem for nUVR sets of matrices
*/
   int i;

   if(nUVR>6) error2("maximum number of matrices set to 6.");

   PMat=(double*)malloc((nc*nc+nUVR*nc*nc*2+nUVR*nc)*sizeof(double));
   if(PMat==NULL) error2("oom getting P&U&V&Root");
   U=_UU[0]=PMat+nc*nc;  V=_VV[0]=_UU[0]+nc*nc; Root=_Root[0]=_VV[0]+nc*nc;
   for(i=1; i<nUVR; i++) {
      _UU[i]=_UU[i-1]+nc*nc*2+nc; _VV[i]=_VV[i-1]+nc*nc*2+nc; 
      _Root[i]=_Root[i-1]+nc*nc*2+nc;
   }
   return(0);
}

void FreeMemPUVR(void)
{   
   free(PMat); 
}


int GetUVRoot_codeml (void)
{
/* This uses data.daafile[] to set up the eigen matrices U, V, Root for 
   combined clock analyses of multiple protein data sets (clock = 5 or 6).
*/
   int locus, nc=(com.seqtype==1?64:20), nUVR=data.ngene;

   if(com.seqtype==1 && (!com.fix_kappa || !com.fix_omega)) nUVR=1;
   GetMemPUVR(nc, nUVR);

   if(nUVR>6) error2("The maximum number of proteins is set to 6.");
   if(com.seqtype==2) {
      for(locus=0; locus<data.ngene; locus++) {
         if(data.ngene>1) 
            strcpy(com.daafile, data.daafile[locus]);
         GetDaa(NULL, com.daa);
         if(com.model==Empirical_F) 
            xtoy(data.pi[locus], com.pi, nc);
         EigenQaa(NULL, _Root[locus], _UU[locus], _VV[locus], NULL);

printf("Protein # %2d uses %-20s\n", locus+1,data.daafile[locus]);
matout(F0, com.pi, 1, nc);
matout(F0, _Root[locus], 1, nc);
      }
   }
   else if(com.seqtype==1 && com.fix_kappa & com.fix_omega) {
      for(locus=0; locus<data.ngene; locus++) {
         if(com.seqtype==1) {
            com.icode=data.icode[locus];
            setmark_61_64 ();
         }
         com.kappa=data.kappa[locus];
         com.omega=data.omega[locus];
         xtoy(data.pi[locus], com.pi, com.ncode);
         EigenQc(0,-1,NULL,NULL,NULL, _Root[locus], _UU[locus], _VV[locus], 
            &com.kappa, com.omega, PMat);
      }
   }
   return(0);
}


#endif


int UseLocus (int locus, int copycondP, int setmodel, int setSeqName)
{
/* This point nodes to the gene tree at locus gnodes[locus] and set com.z[] 
   etc. for likelihood calculation for the locus.  
*/
   int i, nS;

   com.ns=data.ns[locus]; com.ls=data.ls[locus];
   tree.root=data.root[locus];
   tree.nnode=2*com.ns-1;  /* assumes binary tree */
   tree.nbranch=tree.nnode-1;

   nodes=gnodes[locus];

   com.cleandata=data.cleandata[locus];
   com.npatt=com.posG[1]=data.npatt[locus];  com.posG[0]=0;
   com.fpatt=data.fpatt[locus];
   for(i=0; i<com.ns; i++) com.z[i]=data.z[locus][i];

   /* The following is model-dependent */
   if(setmodel) {

      com.kappa=data.kappa[locus];
      com.omega=data.omega[locus];
      com.alpha=data.alpha[locus];

#if(defined CODEML)
      if(com.seqtype==1) {
         com.icode=data.icode[locus];
         setmark_61_64 ();
      }
#endif

#if(defined BASEML)
      if(com.seqtype==0 && com.model!=0 && com.model!=1)
         xtoy(data.pi[locus], com.pi, com.ncode);
      if(com.model<=TN93)
         EigenTN93(com.model, com.kappa, com.kappa, com.pi, &nR, Root, Cijk);
      else if (com.model==REV)
         EigenQREVbase (NULL, &com.kappa, com.pi, &nR, Root, Cijk);
#else
      if((com.seqtype==1 && com.codonf) || (com.seqtype==2 && com.model==3))
         xtoy(data.pi[locus], com.pi, com.ncode);

      if((com.seqtype==2 && (com.model==2 || com.model==3))
         || (com.seqtype==1 && com.fix_kappa && com.fix_omega)) {
         Root=_Root[locus]; U=_UU[locus];  V=_VV[locus];
      }
      else {
         EigenQc(0,-1,NULL,NULL,NULL,Root,U,V, &com.kappa, com.omega,PMat);
      }

#endif
      if(com.alpha)
         DiscreteGamma (com.freqK,com.rK,com.alpha,com.alpha,com.ncatG,0);

      com.NnodeScale=data.NnodeScale[locus];
      com.nodeScale=data.nodeScale[locus];
      nS = com.NnodeScale*com.npatt * (com.conPSiteClass?com.ncatG:1);
      FOR(i,nS) com.nodeScaleF[i]=0;
   }
   if(setSeqName)
      FOR(i,com.ns) com.spname[i]=sptree.nodes[nodes[i].ipop].name;
   return(0);
}


void GetMemBC ()
{
/* This gets mem, for baseml and codeml, local clock models for combined data.
   com.conP[] is shared across loci.
   com.conP0[] allocates space for each gene tree for each locus.
   fhK[] uses shared space for loci.
*/
   int j, locus, sconP0=0, sfhK=0, snode, s1, nc=(com.seqtype==1?64:com.ncode);
   int maxsizeScale=0,nS;
   double *p;

   for(locus=0,com.sconP=sconP0=0; locus<data.ngene; locus++) {
      snode=nc*data.npatt[locus];
      s1 = snode*(data.ns[locus]-1)*sizeof(double);
      if(com.alpha) {     /* this is for step 1, using method = 1 */
         com.conPSiteClass = 1;
         s1 *= com.ncatG;
      }
      if(s1>com.sconP) com.sconP=s1;
      if(!data.cleandata[locus])
         sconP0 += snode*data.ns[locus]*sizeof(double);
      if(com.alpha && data.npatt[locus]>sfhK) 
         sfhK=data.npatt[locus];
   }

   com.conP =(double*)malloc(com.sconP);
   if(sconP0) com.conP0=(double*)malloc(sconP0);
   printf("\n%5d bytes for conP0, %5d bytes for conP1\n",sconP0,com.sconP);
 
   if(com.conP==NULL || (sconP0 && com.conP0==NULL)) 
      error2("oom conP || conP0");
   if (com.alpha) {
      sfhK *= com.ncatG*sizeof(double);
      if((com.fhK=(double*)realloc(com.fhK,sfhK))==NULL) error2("oom");
   }

   /* set gnodes[locus][].conP for tips and internal nodes */
   for(locus=0; locus<data.ngene; locus++) {
      snode=nc*data.npatt[locus];
      for(j=data.ns[locus]; j<data.ns[locus]*2-1; j++)
         gnodes[locus][j].conP = com.conP+(j-data.ns[locus])*snode;
   }
   for(locus=0,p=com.conP0; locus<data.ngene; locus++) {
      snode=nc*data.npatt[locus];
      if(!data.cleandata[locus]) {
         for(j=0; j<data.ns[locus]; j++,p+=snode)
            gnodes[locus][j].conP = p;
         UseLocus(locus, -1, 0, 0);
         InitConditionalPNode();
      }
   }

   if(sptree.nspecies>20) {
      for(locus=0; locus<data.ngene; locus++) {
         UseLocus(locus, -1, 0, 0);
         com.NnodeScale=0;
         com.nodeScale=data.nodeScale[locus]=(char*)malloc(tree.nnode*sizeof(char));
         if(data.nodeScale==NULL)  error2("oom");
         FOR(j,tree.nnode) com.nodeScale[j]=0;

         SetNodeScale(tree.root);

         data.NnodeScale[locus]=com.NnodeScale;
         nS=com.NnodeScale*com.npatt;
         if(com.conPSiteClass) nS*=com.ncatG;
         maxsizeScale=max2(maxsizeScale,nS);

         if(com.NnodeScale) {
            printf("\n%d node(s) used for scaling at locus %d: \n",com.NnodeScale,locus+1);
            FOR(j,tree.nnode) if(com.nodeScale[j]) printf(" %2d",j+1);
            FPN(F0);
         }
      }
      if(maxsizeScale) {
         if((com.nodeScaleF=(double*)malloc(maxsizeScale*sizeof(double)))==NULL)
            error2("oom nscale");
         FOR(j,maxsizeScale) com.nodeScaleF[j]=0;
      }
   }

}

void FreeMemBC (void)
{
   int locus, j;

   FOR(locus,data.ngene) free(gnodes[locus]);
   free(gnodes);
   free(com.conP);
   free(com.conP0);
   for(locus=0; locus<data.ngene; locus++) {
      free(data.fpatt[locus]);
      for(j=0;j<data.ns[locus]; j++)
         free(data.z[locus][j]);
   }
   if(com.alpha)
      free(com.fhK);

   if(sptree.nspecies>20) {
      for(locus=0; locus<data.ngene; locus++)
         free(data.nodeScale);
      if(com.nodeScaleF) free(com.nodeScaleF);
   }
}




double nu_AHRS=0.001, *varb_AHRS;


double funSS_AHRS(double x[], int np);


double lnLfunHeteroData (double x[], int np)
{
/* This calculates the log likelihood, the log of the probability of the data 
   given gtree[] for each locus.  This is for step 3 of Yang (2004. Acta 
   Zoologica Sinica 50:645-656)
   x[0,1,...s-k] has node ages in the species tree, followed by branch rates 
   for genes 1, 2, ..., then kappa for genes, then alpha for genes
*/
   int i,k, locus;
   double lnL=0, lnLt, *pbrate;

   /* ??? need more work for codon sequences */
   for(locus=0,k=com.ntime-1; locus<data.ngene; locus++) 
      k+=data.nbrate[locus];
   if(!com.fix_kappa) FOR(locus,data.ngene) data.kappa[locus]=x[k++];
   if(!com.fix_omega) FOR(locus,data.ngene) data.omega[locus]=x[k++];
   if(!com.fix_alpha) FOR(locus,data.ngene) data.alpha[locus]=x[k++];

   /* update node ages in species tree */
   copySptree();
   SetBranch(x);
   FOR(i,tree.nnode) sptree.nodes[i].age=nodes[i].age;

   for(locus=0,pbrate=x+com.ntime-1; locus<data.ngene; locus++) {

      UseLocus(locus, -1, 1, 1);
      /* copy node ages to gene tree */
      FOR(i,tree.nnode)  nodes[i].age=sptree.nodes[nodes[i].ipop].age;
      FOR(i,tree.nnode) {
         if(i!=tree.root) {
            nodes[i].branch = (nodes[nodes[i].father].age-nodes[i].age) 
                            * pbrate[(int)nodes[i].label];
            if(nodes[i].branch<-1e-4)
               puts("b<0");
         }
      }
      lnL += lnLt=com.plfun(x, -1);
      pbrate+=data.nbrate[locus];
   }
   return(lnL);
}


double funSS_AHRS (double x[], int np)
{
/* Function to be minimized in the ad hoc rate smoothing procedure: 
      lnLb + lnLr
   nodes[].label has node rate.
   lnLb is weighted sum of squares using approximate variances for branch lengths.

   lnLr is the log of the prior of rates under the geometric Brownian motion 
   model of rate evolution. There is no need for recursion as the order at 
   which sptree.nodes are visited is unimportant.  The rates are stored in 
   gnodes[].label.
   The root rate is fixed to be the weighted average rate of its two sons, 
   inversely weighted by the divergence times.
*/
   int locus, j,k, root, pa, son0, son1;
   double lnLb, lnLr, lnLbi, lnLri;  /* lnLb & lnLr are sum of squares for b and r */
   double b,be,t, t0,t1, r,rA, w,y, small=1e-20, smallage=AgeLow[sptree.root]*small;
   double nu = nu_AHRS, *varb=varb_AHRS;

   /* set up node ages in species tree */
   copySptree();
   SetBranch(x);
   FOR(j,tree.nnode) sptree.nodes[j].age=nodes[j].age;

   k=com.ntime-1;
   for(locus=0,lnLb=lnLr=0; locus<data.ngene; varb+=com.ns*2-1,locus++) {
      UseLocus(locus, -1, 0, 0);
      if(data.fix_nu==2)      nu=x[np-1];
      else if(data.fix_nu==3) nu=x[np-1-(data.ngene-1-locus)];

      root=tree.root;
      son0=nodes[root].sons[0];
      son1=nodes[root].sons[1];
      /* copy node ages and rates into gene tree nodes[]. */
      for(j=0; j<tree.nnode; j++) { /* age and rates */
         nodes[j].age=sptree.nodes[nodes[j].ipop].age;
         if(j!=root)
            nodes[j].label = x[k++];
      }
      t0=nodes[root].age-nodes[son0].age;
      t1=nodes[root].age-nodes[son1].age;
      if(t0+t1<1e-7) error2("small root branch.  Think about what to do.");
      nodes[root].label = (nodes[son0].label*t1+nodes[son1].label*t0)/(t0+t1);

      for(j=0,lnLbi=0; j<tree.nnode; j++) {
         if(j==son0 || j==son1) continue;
         pa = nodes[j].father;
         if(j==root) {
            b  = nodes[son0].branch+nodes[son1].branch;
            be = (nodes[j].age-nodes[son0].age) * (nodes[root].label+nodes[son0].label)/2
               + (nodes[j].age-nodes[son1].age) * (nodes[root].label+nodes[son1].label)/2;
         }
         else {
            b  = nodes[j].branch;
            be = (nodes[pa].age-nodes[j].age) * (nodes[pa].label+nodes[j].label)/2;
         }
         w=varb[j];
         if(w<small) 
            puts("small variance");
         lnLbi -= square(be-b)/(2*w);
      }

      for(j=0,lnLri=0; j<tree.nnode; j++) {
         if(j==root) continue;
         pa = nodes[j].father;
         t = nodes[pa].age - nodes[j].age;
         t = max2(t,smallage);
         r = nodes[j].label;
         rA= nodes[pa].label;

         if(rA<small || t<small || r<small)  puts("small r, rA, or t");
         y = log(r/rA)+t*nu/2;
         lnLri -= y*y/(2*t*nu) - log(r) - log(2*PI*t*nu)/2;
      }

      if(data.fix_nu>1) lnLri += -nu/nu_AHRS-log(nu);  /* exponential prior */
      lnLb -= lnLbi;
      lnLr -= lnLri;
   }
   return (lnLb + lnLr);
}


void SetBranchRates(int inode)
{
/* this uses node rates to set branch rates, and is used only after the ad hoc 
   rate smoothing iteration is finished.
*/
   int i;
   if(inode<com.ns) 
      nodes[inode].label = (nodes[inode].label + nodes[nodes[inode].father].label)/2;
   else
      for(i=0; i<nodes[inode].nson; i++) 
         SetBranchRates(nodes[inode].sons[i]);
}


int GetInitialsClock6Step1 (double x[], double xb[][2])
{
/* This is for clock 6 step 1.
*/
   int i,k;
   double tb[]={.0001, 999};

   com.ntime=k=tree.nbranch;
   GetInitialsTimes (x);

   com.plfun = (com.alpha==0 ? lfun : lfundG);
   com.conPSiteClass = (com.method && com.plfun==lfundG);

/*   InitializeNodeScale(); */

   if(com.seqtype==0)  com.nrate = !com.fix_kappa;

   com.np=com.ntime+!com.fix_kappa+!com.fix_alpha;
   if(com.seqtype==1 && !com.fix_omega) com.np++;

   if(!com.fix_kappa) x[k++]=com.kappa;
   if(!com.fix_omega) x[k++]=com.omega;
   if(!com.fix_alpha) x[k++]=com.alpha;
   NodeToBranch ();
   
   for(i=0; i<com.ntime; i++)  
      { xb[i][0]=tb[0]; xb[i][1]=tb[1]; }
   for( ; i<com.np; i++)  
      { xb[i][0]=.001; xb[i][1]=999; }

   if(noisy>3 && com.np<200) {
      printf("\nInitials (np=%d)\n", com.np);
      for(i=0; i<com.np; i++) printf(" %10.5f", x[i]);      FPN(F0);
      for(i=0; i<com.np; i++) printf(" %10.5f", xb[i][0]);  FPN(F0);
      for(i=0; i<com.np; i++) printf(" %10.5f", xb[i][1]);  FPN(F0);
   }
   return (0);
}



int GetInitialsClock56Step3 (double x[])
{
/* This is for clock 5 or clock 6 step 3
*/
   int i, j,k=0, naa=20;

   if(com.clock==5)
      GetInitialsTimes (x);

   com.plfun = (com.alpha==0 ? lfun : lfundG);
   com.conPSiteClass = (com.method && com.plfun==lfundG);

/*   InitializeNodeScale(); */

   com.np = com.ntime-1 + (1+!com.fix_kappa+!com.fix_omega+!com.fix_alpha)*data.ngene;
   if(com.clock==5) 
      for(i=com.ntime-1;i<com.np;i++) x[i]=.2+rndu();
   else if(com.clock==6) {
      for(j=0,k=com.ntime-1; j<data.ngene; k+=data.nbrate[j],j++) 
         com.np += data.nbrate[j]-1;
      if(!com.fix_kappa)
         for(j=0; j<data.ngene; j++) x[k++]=data.kappa[j];
      if(!com.fix_omega) 
         for(j=0; j<data.ngene; j++) x[k++]=data.omega[j];
      if(!com.fix_alpha) 
         for(j=0; j<data.ngene; j++) x[k++]=data.alpha[j];
      for(i=k;i<com.np;i++) x[i]=(.5+rndu())/2;
   }
   return (0);
}


double GetMeanRate (void)
{
/* This gets the rough average rate for the locus 
*/
   int inode, i,j,k, ipop, nleft,nright,marks[NS], sons[2], nfossil;
   double mr, md;

   mr=0; nfossil=0;
   for(inode=com.ns; inode<tree.nnode; inode++) {
      ipop=nodes[inode].ipop;  
      if(sptree.nodes[ipop].fossil==0) continue;
      sons[0]=nodes[inode].sons[0];
      sons[1]=nodes[inode].sons[1];
      for(i=0,nleft=nright=0; i<com.ns; i++) {
         for(j=i,marks[i]=0; j!=tree.root; j=nodes[j].father) {
            if(j==sons[0])       { marks[i]=1; nleft++;  break; }
            else if (j==sons[1]) { marks[i]=2; nright++; break; }
         }
      }
      if(nleft==0 || nright==0) {
         puts("this calibration is not in gene tree.");
         continue;
      }
      nfossil++;

      for(i=0,md=0; i<com.ns; i++) {
         for(j=0; j<com.ns; j++) {
            if(marks[i]==1 && marks[j]==2) {
               for(k=i; k!=inode; k=nodes[k].father)
                  md+=nodes[k].branch;
               for(k=j; k!=inode; k=nodes[k].father)
                  md+=nodes[k].branch;
            }
         }
      }
      md/=(nleft*nright);
      mr+=md/(sptree.nodes[ipop].age*2);

      printf("node age & mr n%-4d %9.5f%9.5f  ", inode, sptree.nodes[ipop].age, md);
      if(com.ns<100) FOR(i,com.ns) printf("%d",marks[i]); 
      FPN(F0);

   }
   mr/=nfossil;
   if(nfossil==0) { printf("need fossils for this locus\n"); exit(-1); }

   return(mr);
}


int AdHocRateSmoothing (FILE*fout, double x[NS*3], double xb[NS*3][2], double space[])
{
/* ad hoc rate smoothing for likelihood estimation of divergence times.
   Step 1: Use JC69 to estimate branch lengths under no-clock model.
   Step 2: ad hoc rate smoothing, estimating one set of divergence times
           and many sets of branch rates for loci.  Rate at root is set to 
           weighted average of rate at the two sons.
*/
   int model0=com.model, ntime0=com.ntime;  /* is this useful? */
   int fix_kappa0=com.fix_kappa, fix_omega0=com.fix_omega, fix_alpha0=com.fix_alpha;
   int son0, son1;
   double kappa0=com.kappa, omega0=com.omega, alpha0=com.alpha, t0,t1, *varb;
   double f, e=1e-8, pb=0.00001, rb[]={0.001,99}, lnL,lnLsum=0;
   double mbrate[20], Rj[20], r,minr,maxr, beta, *pnu=&nu_AHRS,nu, mr[NGENE];
   int i,j,k,k0, locus, nbrate[20],maxnbrate=20;
   char timestr[32];
   FILE *fBV=gfopen("in.BV","w");
   FILE *fdist=gfopen("RateDist.txt","w");

   FILE *finStep1=fopen("in.ClockStep1","r"), *finStep2=fopen("in.ClockStep2","r");


   noisy=4;
   for(locus=0,k=0; locus<data.ngene; locus++)  k+=2*data.ns[locus]-1;
   if((varb_AHRS=(double*)malloc(k*sizeof(double)))==NULL) 
      error2("oom AHRS");
   for(i=0; i<k;i++)  varb_AHRS[i]=-1;


   /* Step 1: Estimate branch lengths without clock.  */
   printf("\nStep 1: Estimate branch lengths under no clock.\n");
   fprintf(fout,"\n\nStep 1: Estimate branch lengths under no clock.\n");
   com.clock=0; com.method=1;
/*
com.model=0;  com.fix_kappa=1; com.kappa=1; 
com.fix_alpha=1; com.alpha=0;
*/
   for(locus=0; locus<data.ngene; locus++) {
      if(!com.fix_kappa) data.kappa[locus]=com.kappa;
      if(!com.fix_omega) data.omega[locus]=com.omega;
      if(!com.fix_alpha) data.alpha[locus]=com.alpha;
   }
   for(locus=0,varb=varb_AHRS; locus<data.ngene; varb+=com.ns*2-1,locus++) {
      UseLocus(locus, -1, 1, 1);

      fprintf(fout,"\nLocus %d (%d sequences)\n", locus+1, com.ns);

      son0=nodes[tree.root].sons[0]; son1=nodes[tree.root].sons[1];

      GetInitialsClock6Step1 (x, xb);

      lnL=0;
      if(com.ns>30) fprintf(frub, "\n\nLocus %d\n", locus+1);
      if(finStep1) {
         puts("read MLEs from step 1 from file");
         for(i=0; i<com.np; i++) fscanf(finStep1,"%lf",&x[i]);
      }
      else
         j=minB((com.ns>30?frub:NULL), &lnL,x,xb, e, space);

      if(!com.fix_kappa) data.kappa[locus]=x[com.ntime];
      if(!com.fix_omega) data.omega[locus]=x[com.ntime+!com.fix_kappa];
      if(!com.fix_alpha) data.alpha[locus]=x[com.ntime+!com.fix_kappa+!com.fix_omega];

      lnLsum += lnL;
      for(j=0; j<2*com.ns-1; j++)  varb[j]=space[j];
      t0=nodes[son0].branch; t1=nodes[son1].branch;
      varb[tree.root]=varb[t0>t1?son0:son1];
      nodes[son0].branch=nodes[son1].branch = (t0+t1)/2;  /* arbitrary */
      mr[locus]=GetMeanRate();

      printf("   Locus %d: %d sequences, %d blengths, lnL = %15.6f mr=%.5f%10s\n", 
         locus+1, com.ns, com.np-1,-lnL,mr[locus], printtime(timestr));
      fprintf(fout,"\nlnL = %.6f\n\n", -lnL);
      OutaTreeB(fout);  FPN(fout);
      for(i=0; i<com.np; i++) fprintf(fout," %8.5f",x[i]); FPN(fout);
      for(i=0; i<tree.nbranch; i++) fprintf(fout," %8.5f", sqrt(varb[tree.branches[i][1]])); FPN(fout);
      FPN(fout);  OutaTreeN(fout,1,1);  FPN(fout);  fflush(fout);

      fprintf(fBV, "\n\nLocus %d: %d sequences, %d+1 branches\nlnL = %15.6f\n\n", 
         locus+1, com.ns, tree.nbranch-1, -lnL);
      OutaTreeB(fBV);  FPN(fBV);
      for(i=0; i<tree.nbranch; i++) fprintf(fBV," %12.9f",x[i]); FPN(fBV);
      for(i=0; i<tree.nbranch; i++) fprintf(fBV," %12.9f", sqrt(varb[tree.branches[i][1]])); FPN(fBV);
      FPN(fBV);  OutaTreeN(fBV,1,1);  FPN(fBV);  fflush(fBV);

   }
   fclose(fBV);
   if(data.ngene>1) fprintf(fout,"\nSum of lnL over loci = %15.6f\n", -lnLsum);


   /* Step 2: ad hoc rate smoothing to estimate branch rates.  */
   printf("\nStep 2: Ad hoc rate smoothing to estimate branch rates.\n");
   fprintf(fout, "\n\nStep 2: Ad hoc rate smoothing to estimate branch rates.\n");
   /* s - 1 - NFossils node ages, (2*s_i - 2) rates for branches at each locus */
   com.clock=1;
   copySptree();
   GetInitialsTimes (x);

   for(locus=0,com.np=com.ntime-1; locus<data.ngene; locus++) 
      com.np += data.ns[locus]*2-2;
   if(data.fix_nu==2) com.np++;
   if(data.fix_nu==3) com.np+=data.ngene;

   if(com.np>NS*6) error2("change NP for ad hoc rate smoothing.");
   for(i=0; i<com.ntime-1; i++)
      { xb[i][0]=pb;  xb[i][1]=1-pb; }
   if(!nodes[tree.root].fossil)  
      { xb[0][0]=AgeLow[tree.root]*1.0001; xb[0][1]=max2(AgeLow[tree.root]*10,50); }
   for( ; i<com.np; i++)  { /* for rates */
      xb[i][0]=rb[0]; xb[i][1]=rb[1];
   }
   for(locus=0,i=com.ntime-1; locus<data.ngene; locus++) 
      for(j=0; j<data.ns[locus]*2-2; j++) 
         x[i++]=mr[locus]*(.8+.4*rndu());
   for( ; i<com.np; i++)   /* nu */
      x[i]=0.001+0.1*rndu();

   if(noisy>3) {
      for(i=0; i<com.np; i++) 
         { printf(" %10.5f", x[i]); if(i==com.ntime-2) FPN(F0); }  FPN(F0);
      if(com.np<200) {
         for(i=0; i<com.np; i++)  printf(" %10.5f", xb[i][0]);  FPN(F0);
         for(i=0; i<com.np; i++)  printf(" %10.5f", xb[i][1]);  FPN(F0);
      }
   }

   if(data.fix_nu>1) 
      pnu = x+com.np-(data.fix_nu==2 ? 1 : data.ngene);
   printf("  %d times, %d rates, %d parameters, ", com.ntime-1,k,com.np);

   noisy=3;
   f = funSS_AHRS(x, com.np);
   if(noisy>2) printf("\nf0 = %12.6f\n",f );

   if(finStep2) {
      puts("read MLEs from step 2 from file");
      for(i=0; i<com.np; i++) fscanf(finStep2,"%lf",&x[i]);
      matout(F0,x,1,com.np);
   }
   else {
      j = ming2(frub, &f, funSS_AHRS, NULL, x, xb, space, 1e-9, com.np);

      /* generate output to in.clockStep2
      matout(fout,x,1,com.np);
      */

      if(j==-1) 
         { puts("\nad hoc rate smoothing iteration may not have converged.\nEnter to continue; Ctrl-C to break."); 
      getchar(); }
   }
   free(varb_AHRS);

   fputs("\nEstimated divergence times from ad hoc rate smoothing\n\n",fout);
   copySptree();
   FOR(i,tree.nnode) nodes[i].branch*=100;
   for(i=com.ns; i<tree.nnode; i++)
      fprintf(fout, "Node %2d   Time %9.5f\n", i+1, nodes[i].age*100);
   FPN(fout); OutaTreeN(fout,1,1); FPN(fout);

   fprintf(fout, "\nEstimated rates from ad hoc rate smoothing\n");
   for(locus=0,k=k0=com.ntime-1; locus<data.ngene; k0+=data.nbrate[locus++]) {

      UseLocus(locus, -1, 0, 1);
      for(i=0; i<tree.nnode; i++)
         if(i!=tree.root)  nodes[i].label=x[k++];
      son0=nodes[tree.root].sons[0]; son1=nodes[tree.root].sons[1];
      t0=nodes[tree.root].age-nodes[son0].age; 
      t1=nodes[tree.root].age-nodes[son1].age; 
      nodes[tree.root].label = (nodes[son0].label*t1+nodes[son1].label*t0)/(t0+t1);
      SetBranchRates(tree.root);  /* node rates -> branch rates */

      nu = (data.fix_nu==3 ? *(pnu+locus) : *pnu);
      fprintf(fout,"\nLocus %d (%d sequences)\n\n", locus+1, com.ns);
      fprintf(fout,"nu = %.6g\n", nu);

      /* this block can be deleted? */
      fprintf(fout, "\nnode \tage \tlength \trate\n");
      for(i=0; i<tree.nnode; i++,FPN(fout)) {
         fprintf(fout, "%02d\t%.3f", i+1,nodes[i].age);
         if(i!=tree.root) 
            fprintf(fout, "\t%.5f\t%.5f", nodes[i].branch,nodes[i].label);
      }

      fprintf(fout,"\nRates as labels in tree:\n"); 
      OutaTreeN(fout,1,3); FPN(fout);  fflush(fout);

      if(data.nbrate[locus]>maxnbrate) error2("too many rate classes?  Change source.");
      for(i=0,minr=1e6,maxr=0; i<tree.nnode; i++)
         if(i!=tree.root) {
            r=nodes[i].label;
            if(r<0 && noisy) 
               puts("node label<0?");
            minr = min2(minr,r);
            maxr = max2(maxr,r);
         }

      fprintf(fdist, "\n%6d\n", tree.nnode-1);
      for(i=0; i<tree.nnode; i++) {
         if(i==tree.root) continue;
         fprintf(fdist, "R%-10.7f  ", nodes[i].label);
         for(j=0; j<i; j++)
            if(j!=tree.root)
               fprintf(fdist, " %9.6f", fabs(nodes[i].label-nodes[j].label));
         FPN(fdist);
      }
      fflush(fdist);
/*
      for(j=0; j<data.nbrate[locus]; j++)
         Rj[j]=minr+(j+1)*(maxr-minr)/data.nbrate[locus];
*/
      beta = pow(1./(data.nbrate[locus]+1), 1/(data.nbrate[locus]-1.));
      beta = 0.25+0.25*log((double)data.nbrate[locus]);
      if(beta>1) beta=0.99;
      for(j=0; j<data.nbrate[locus]; j++)
         Rj[j]=minr+(maxr-minr)*pow(beta,(double)data.nbrate[locus]-1-j);

printf("\nLocus %d: nu = %.6f, rate range (%.6f, %.6f)\n", locus+1,nu,minr,maxr);
printf("Cutting points:\n");
for(j=0; j<data.nbrate[locus]; j++)
   printf(" < %.6f, ", Rj[j]);
printf("\nThe number of rate groups (0 for no change)? ");
/* scanf("%d", &j); */
j=0;
if(j) {
   data.nbrate[locus]=j;
   printf("input %d cutting points? ", data.nbrate[locus]-1);
   for(j=0,Rj[data.nbrate[locus]-1]=maxr; j<data.nbrate[locus]-1; j++)
      scanf("%lf", &Rj[j]);
}

      for(i=0;i<data.nbrate[locus];i++) { mbrate[i]=0; nbrate[i]=0; }
      for(i=0; i<tree.nnode; i++) {
         if(i==tree.root) continue;
         r=nodes[i].label;
         for(j=0; j<data.nbrate[locus]-1; j++)
            if(r<Rj[j]) break;
         mbrate[j] += r;
         nbrate[j] ++;
         nodes[i].label = j;
      }
      nodes[tree.root].label=-1;
      for(i=0;i<data.nbrate[locus];i++) 
         mbrate[i] = (nbrate[i]?mbrate[i]/nbrate[i]:-1);

      fprintf(fout,"\nCollapsing rates into groups\nRate range: (%.6f, %.6f)\n", minr,maxr);
/*      fprintf(fout,"\nCollapsing rates into groups\nbeta = %.6g  Rate range: (%.6f, %.6f)\n", beta, minr,maxr);
*/
      for(j=0; j<data.nbrate[locus]; j++)
         fprintf(fout,"rate group %d  (%2d): <%9.6f, mean %9.6f\n", 
            j, nbrate[j], Rj[j], mbrate[j]);

      FPN(fout); OutaTreeN(fout,1,3); FPN(fout);
      fprintf(fout, "\n\nRough rates for branch groups at locus %d\n", locus+1);
      for(i=0; i<data.nbrate[locus]; i++)
         x[k0+i] = mbrate[i];
   }

printf("\n\n%d times, %d timerates from AHRS:\n", com.ntime-1,k0);
fprintf(fout,"\n\n%d times, %d timerates from AHRS\n", com.ntime-1,k0);
for(i=0; i<k0; i++) {
   printf("%12.6f", x[i]);
   if(i==com.ntime-2) FPN(F0);
   fprintf(fout,"%12.6f", x[i]);
   if(i==com.ntime-2) FPN(fout);
}
FPN(F0);  FPN(fout);

   for(i=0; i<k0; i++) x[i]*=0.9+0.2*rndu(); 
   
   com.model=model0;  com.clock=6;  


   com.fix_kappa=fix_kappa0; com.kappa=kappa0;
   com.fix_omega=fix_omega0; com.omega=omega0;
   com.fix_alpha=fix_alpha0; com.alpha=alpha0;

#if 0
   /* fix parameters: value > 0, precise value unimportant */
   if(!fix_kappa0) { com.fix_kappa=1; com.kappa=0.1; }
   if(!fix_omega0) { com.fix_omega=1; com.omega=0.1; }
   if(!fix_alpha0) { com.fix_alpha=1; com.alpha=0.1; }
#endif

   fclose(fdist);
   fflush(fout);
   printf(" %10s\n", printtime(timestr));

   if(finStep1) fclose(finStep1);
   if(finStep2) fclose(finStep2);

   return(0);
}


void DatingHeteroData (FILE* fout)
{
/* This is for clock and local-clock dating using heterogeneous data from 
   multiple loci.  Some species might be missing at some loci.  Thus 
   gnodes[locus] stores the gene tree at locus.  Branch lengths in the gene 
   tree are constructed using the divergence times in the master species tree, 
   and the rates for genes and branches.  

      com.clock = 5: global clock
                  6: local clock
*/
   char timestr[64];
   int i,j,k, s, np, sconP0=0, locus;
   int maxnpML, maxnpADRS;
   double x[NS*6],xb[NS*6][2], lnL,e=1e-7, *var=NULL;
   int nbrate=4;


data.fix_nu=3;
/*
if(com.clock==6) {
  printf("nu (1:fix; 2:estimate one for all genes; 3:estimate one for every gene)? ");
  scanf("%d", &data.fix_nu);
  if(data.fix_nu==1) scanf("%lf", &nu_AHRS);
}
*/
   ReadTreeSeqs(fout);
   com.nbtype=1;
   for(j=0; j<sptree.nnode; j++) {
      sptree.nodes[j].tfossil[0]=sptree.nodes[j].tfossil[1]=-1;
   }
   for(j=sptree.nspecies,com.ntime=j-1,sptree.nfossil=0; j<sptree.nnode; j++) {
      if(sptree.nodes[j].fossil) {
         com.ntime--;
         sptree.nfossil++;
         printf("node %2d age fixed at %.3f\n", j, sptree.nodes[j].age);
      }
   }
   GetMemBC();
   s=sptree.nspecies;
   maxnpML = s-1 + (5+2)*data.ngene;
   maxnpADRS = s-1 + (2*s-1)*data.ngene + 2*data.ngene;
   com.sspace=max2(com.sspace, spaceming2(maxnpADRS));
   com.sspace=max2(com.sspace, maxnpML*(maxnpML+1)*(int)sizeof(double));
   if((com.space=(double*)realloc(com.space,com.sspace))==NULL) 
      error2("oom space");

#if (defined CODEML)
   GetUVRoot_codeml ();
#endif
   if(com.clock==6) {
      if(data.fix_nu<=1) {
         printf("nu & nbrate? ");
         scanf("%lf%d? ", &nu_AHRS, &nbrate);
      }
      for(locus=0; locus<data.ngene; locus++)  data.nbrate[locus]=nbrate;
      AdHocRateSmoothing(fout, x, xb, com.space);

      printf("\nStep 3: ML estimation of times and rates.");
      fprintf(fout,"\n\nStep 3: ML estimation of times and rates.\n");
   }
   else {   /* clock = 5, global clock */
      for(locus=0; locus<data.ngene; locus++) 
         for(i=0,data.nbrate[locus]=1; i<data.ns[locus]*2-1; i++)
            gnodes[locus][i].label=0;
   }

   noisy=3;

   copySptree();
   GetInitialsClock56Step3(x);
   np=com.np;

   SetxBound (com.np, xb);
   lnL = lnLfunHeteroData(x,np);

   if(noisy) {
      printf("\nntime & nrate & np:%6d%6d%6d\n",com.ntime-1,com.nrate,com.np);
      matout(F0,x,1,np);
      printf("\nlnL0 = %12.6f\n",-lnL);
   }

   j=ming2(noisy>2?frub:NULL,&lnL,lnLfunHeteroData,NULL,x,xb, com.space,e,np);

   if(noisy) printf("Out...\nlnL  = %12.6f\n", -lnL);
   
   LASTROUND=1;
   for(i=0,j=!sptree.nodes[sptree.root].fossil; i<sptree.nnode; i++) 
      if(i!=sptree.root && sptree.nodes[i].nson && !sptree.nodes[i].fossil) 
         x[j++]=sptree.nodes[i].age;       /* copy node ages into x[] */

   if (com.getSE) {
      if(np>100 || (com.seqtype && np>20)) puts("Calculating SE's");
      var=com.space+np;
      Hessian (np,x,lnL,com.space,var,lnLfunHeteroData,var+np*np);
      matinv(var,np,np,var+np*np);
   }
   copySptree();
   SetBranch(x);
   fprintf(fout,"\n\nTree:  ");  OutaTreeN(fout,0,0);
   fprintf(fout,"\nlnL(ntime:%3d  np:%3d):%14.6f\n", com.ntime-1,np,-lnL);
   OutaTreeB(fout);  FPN (fout);
   for(i=0;i<np;i++) fprintf(fout," %9.5f",x[i]);  FPN(fout);  fflush(fout);

   if(com.getSE) {
      fprintf(fout,"SEs for parameters:\n");
      for(i=0;i<np;i++) fprintf(fout," %9.5f",(var[i*np+i]>0.?sqrt(var[i*np+i]):-1));
      FPN(fout);
      if (com.getSE==2) matout2(fout, var, np, np, 15, 10);
   }

   fprintf(fout,"\nTree with node ages for TreeView\n");
   FOR(i,tree.nnode) nodes[i].branch*=100;
   FPN(fout);  OutaTreeN(fout,1,1);  FPN(fout);
   FPN(fout);  OutaTreeN(fout,1,2);  FPN(fout);
   FPN(fout);  OutaTreeN(fout,1,5);  FPN(fout);
   FPN(fout);  OutaTreeN(fout,1,0);  FPN(fout);
   OutputTimesRates(fout, x, var);

   fprintf(fout,"\nSubstititon rates for genes (per time unit)\n");
   for(j=0,k=com.ntime-1; j<data.ngene; j++,FPN(fout)) {
      fprintf(fout,"   Gene %2d: ", j+1);
      for(i=0; i<data.nbrate[j]; i++,k++) {
         fprintf(fout,"%10.5f", x[k]);
         if(com.getSE) fprintf(fout," +- %.5f", sqrt(var[k*np+k]));
      }
      if(com.clock==6) fprintf(fout," ");
   }
   if(!com.fix_kappa) {
      fprintf(fout,"\nkappa for genes\n");
      for(j=0; j<data.ngene; j++,k++) {
         fprintf(fout,"%10.5f", data.kappa[j]);
         if(com.getSE) fprintf(fout," +- %.5f", sqrt(var[k*np+k]));
      }
   }
   if(!com.fix_omega) {
      fprintf(fout,"\nomega for genes\n");
      for(j=0; j<data.ngene; j++,k++) {
         fprintf(fout,"%10.5f", data.omega[j]);
         if(com.getSE) fprintf(fout," +- %.5f", sqrt(var[k*np+k]));
      }
   }
   if(!com.fix_alpha) {
      fprintf(fout,"\nalpha for genes\n");
      for(j=0; j<data.ngene; j++,k++) {
         fprintf(fout,"%10.5f", data.alpha[j]);
         if(com.getSE) fprintf(fout," +- %.5f", sqrt(var[k*np+k]));
      }
   }
   FPN(fout);
   FreeMemBC();
   printf("\nTime used: %s\n", printtime(timestr));
   exit(0);
}

#endif
