/*****************************************************************
*        This code is a part of the CFD-with-CUDA project        *
*             http://code.google.com/p/cfd-with-cuda             *
*                                                                *
*              Dr. Cuneyt Sert and Mahmut M. Gocmen              *
*                                                                *
*              Department of Mechanical Engineering              *
*                Middle East Technical University                *
*                         Ankara, Turkey                         *
*            http://www.me.metu.edu.tr/people/cuneyt             *
*****************************************************************/

#include <stdio.h>
#include <string>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <ctime>
#include <cmath>
#include <algorithm>

using namespace std;

#ifdef SINGLE
  typedef float real;
#else
  typedef double real;
#endif

ifstream meshfile;
ifstream problemFile;
ofstream outputFile;
ofstream outputControl;

string problemNameFile, whichProblem;
string problemName = "ProblemName.txt";
string controlFile = "Control_Output.txt";
string inputExtension  = ".inp";              // Change this line to specify the extension of input file (with dot).
string outputExtension  = ".dat";             // Change this line to specify the extension of output file (with dot).

int name, eType, NE, NN, NGP, NEU, Ndof;
int NCN, NENv, NENp, nonlinearIterMax, solverIterMax;        // nonlinearIterMax is not used by the Stokes solver.
double density, viscosity, fx, fy, nonlinearTol, solverTol;  // nonlinearTol is not used by the Stokes solver.
int **LtoG, **velNodes, **pressureNodes;
double **coord;
int nBC, nVelNodes, nPressureNodes;
double *BCtype, **BCstrings;
double axyFunc, fxyFunc;
double **GQpoint, *GQweight;
double **Sp, ***DSp, **Sv, ***DSv;
double **detJacob, ****gDSp, ****gDSv;
real **K, *F, *u;   // Can be float or double. K is the stiffness matrix in full
                    // storage used Gauss Elimination solver.
int bigNumber;

int **GtoL, *rowStarts, *rowStartsSmall, *colSmall, *col, **KeKMapSmall, NNZ; 
real *val;          // Can be float or double

void readInput();
void gaussQuad();
void calcShape();
void calcJacobian();
void calcGlobalSys();
void assemble(int e, double **Ke, double *Fe);
void applyBC();
void solve();
void postProcess();
void gaussElimination(int N, real **K, real *F, real *u, bool& err);
void writeTecplotFile();
void compressedSparseRowStorage();
#ifdef CUSP
   extern void CUSPsolver();
#endif




//------------------------------------------------------------------------------
int main()
//------------------------------------------------------------------------------
{
   cout << "\n *******************************************************";
   cout << "\n *      Stokes 3D - Part of CFD with CUDA project      *";
   cout << "\n *       http://code.google.com/p/cfd-with-cuda        *";
   cout << "\n *******************************************************\n\n";

   cout << "The program is started." << endl ;

   time_t start, end;
   time (&start);     // Start measuring execution time.

   readInput();                   cout << "Input file is read." << endl ;
   compressedSparseRowStorage();  cout << "CSR vectors are created." << endl ;
   gaussQuad();
   calcShape();
   calcJacobian();
   calcGlobalSys();               cout << "Global system is calculated." << endl ;
   applyBC();                     cout << "Boundary conditions are applied." << endl ;
   solve();                       cout << "Global system is solved." << endl ;
   //postProcess();
   writeTecplotFile();            cout << "A DAT file is created for Tecplot." << endl ;

   time (&end);      // Stop measuring execution time.
   cout << endl << "Elapsed wall clock time is " << difftime (end,start) << " seconds." << endl;
   
   cout << endl << "The program is terminated successfully.\nPress a key to close this window...";

   cin.get();
   return 0;

} // End of function main()




//------------------------------------------------------------------------------
void readInput()
//------------------------------------------------------------------------------
{

   string dummy, dummy2, dummy4, dummy5;
   int dummy3, i;

   problemFile.open(problemName.c_str(), ios::in);   // Read problem name
   problemFile >> whichProblem;                                  
   problemFile.close();
   
   meshfile.open((whichProblem + inputExtension).c_str(), ios::in);
     
   meshfile.ignore(256, '\n'); // Read and ignore the line
   meshfile.ignore(256, '\n'); // Read and ignore the line
   meshfile >> dummy >> dummy2 >> eType;
   meshfile.ignore(256, '\n'); // Ignore the rest of the line
   meshfile >> dummy >> dummy2 >> NE;
   meshfile.ignore(256, '\n'); // Ignore the rest of the line
   meshfile >> dummy >> dummy2 >> NCN;
   meshfile.ignore(256, '\n'); // Ignore the rest of the line
   meshfile >> dummy >> dummy2 >> NN;
   meshfile.ignore(256, '\n'); // Ignore the rest of the line
   meshfile >> dummy >> dummy2 >> NENv;
   meshfile.ignore(256, '\n'); // Ignore the rest of the line
   meshfile >> dummy >> dummy2 >> NENp;   
   meshfile.ignore(256, '\n'); // Ignore the rest of the line
   meshfile >> dummy >> dummy2 >> NGP;
   meshfile.ignore(256, '\n'); // Ignore the rest of the line
   meshfile >> dummy >> dummy2 >> nonlinearIterMax;
   meshfile.ignore(256, '\n'); // Ignore the rest of the line
   meshfile >> dummy >> dummy2 >> nonlinearTol;
   meshfile.ignore(256, '\n'); // Ignore the rest of the line
   meshfile >> dummy >> dummy2 >> solverIterMax;
   meshfile.ignore(256, '\n'); // Ignore the rest of the line
   meshfile >> dummy >> dummy2 >> solverTol;
   meshfile.ignore(256, '\n'); // Ignore the rest of the line
   meshfile >> dummy >> dummy2 >> density;
   meshfile.ignore(256, '\n'); // Ignore the rest of the line
   meshfile >> dummy >> dummy2 >> viscosity;
   meshfile.ignore(256, '\n'); // Ignore the rest of the line
   meshfile >> dummy >> dummy2 >> fx;
   meshfile.ignore(256, '\n'); // Ignore the rest of the line
   meshfile >> dummy >> dummy2 >> fy;
   meshfile.ignore(256, '\n'); // Ignore the rest of the line
   meshfile.ignore(256, '\n'); // Read and ignore the line
   meshfile.ignore(256, '\n'); // Read and ignore the line  
   
   NEU = 3*NENv + NENp;
   Ndof = 3*NN + NCN;
   
   // Read node coordinates
   coord = new double*[NN];
   
   if (eType == 2 || eType == 1) {
      for (i = 0; i < NN; i++) {
         coord[i] = new double[2];
      }
   	
      for (i=0; i<NN; i++){
         meshfile >> dummy3 >> coord[i][0] >> coord[i][1] ;
         meshfile.ignore(256, '\n'); // Ignore the rest of the line    
      }  
   } else {
      for (i = 0; i < NN; i++) {
         coord[i] = new double[3];
      }
   	
      for (i=0; i<NN; i++){
         meshfile >> dummy3 >> coord[i][0] >> coord[i][1] >> coord[i][2] ;
         meshfile.ignore(256, '\n'); // Ignore the rest of the line    
      }  
   }
   
   meshfile.ignore(256, '\n'); // Read and ignore the line
   meshfile.ignore(256, '\n'); // Read and ignore the line 
    
   // Read element connectivity, i.e. LtoG
   LtoG = new int*[NE];

   for (i=0; i<NE; i++) {
      LtoG[i] = new int[NENv];
   }

   for (int e = 0; e<NE; e++){
      meshfile >> dummy3;
      for (i = 0; i<NENv; i++){
         meshfile >> LtoG[e][i];
      }
      meshfile.ignore(256, '\n'); // Read and ignore the line 
   } 

   meshfile.ignore(256, '\n'); // Read and ignore the line
   meshfile.ignore(256, '\n'); // Read and ignore the line 

   // Read number of different BC types and details of each BC
   meshfile >> dummy >> dummy2 >> nBC;
   meshfile.ignore(256, '\n'); // Ignore the rest of the line
      
   BCtype = new double[nBC];
   BCstrings = new double*[nBC];
   for (i=0; i<nBC; i++) {
      BCstrings[i] = new double[3];
   }   
    
   for (i = 0; i<nBC-1; i++){
      meshfile >> dummy >> BCtype[i] >> dummy2 >> dummy3 >> BCstrings[i][0] >> dummy4 >> BCstrings[i][1] >> dummy5 >> BCstrings[i][2];
      meshfile.ignore(256, '\n'); // Ignore the rest of the line
   }
   meshfile >> dummy >> BCtype[2]  >> dummy >> BCstrings[i][0];
   meshfile.ignore(256, '\n'); // Ignore the rest of the line
   // Read EBC data 
    
   meshfile.ignore(256, '\n'); // Read and ignore the line 
   meshfile >> dummy >> dummy2 >> nVelNodes;
   meshfile.ignore(256, '\n'); // Ignore the rest of the line
   meshfile >> dummy >> dummy2 >> nPressureNodes;
   meshfile.ignore(256, '\n'); // Ignore the rest of the line
   meshfile.ignore(256, '\n'); // Ignore the rest of the line
   meshfile.ignore(256, '\n'); // Ignore the rest of the line
      
   if (nVelNodes!=0){
      velNodes = new int*[nVelNodes];
      for (i = 0; i < nVelNodes; i++){
         velNodes[i] = new int[2];
      }
      for (i = 0; i < nVelNodes; i++){
         meshfile >> velNodes[i][0] >> velNodes[i][1];   
         meshfile.ignore(256, '\n'); // Ignore the rest of the line
      }
   }

   meshfile.ignore(256, '\n'); // Ignore the rest of the line  
   meshfile.ignore(256, '\n'); // Ignore the rest of the line
   meshfile.ignore(256, '\n'); // Ignore the rest of the line
   
   if (nVelNodes!=0){
      pressureNodes = new int*[nPressureNodes];
      for (i = 0; i < nPressureNodes; i++){
         pressureNodes[i] = new int[2];
      }
      for (i = 0; i < nPressureNodes; i++){
         meshfile >> pressureNodes[i][0] >> pressureNodes[i][1];   
         meshfile.ignore(256, '\n'); // Ignore the rest of the line
      }
   }
  
   meshfile.close();

} // End of function readInput()




//------------------------------------------------------------------------------
void compressedSparseRowStorage()
//------------------------------------------------------------------------------
{
//GtoL creation
int i, j, k, m, x, y, valGtoL, check, temp, *checkCol, noOfColGtoL, *GtoLCounter;

   GtoL = new int*[NN];          // stores which elements connected to the node
   noOfColGtoL = 8;              // for 3D meshes created by our mesh generator max 8 elements connect to one node,
   GtoLCounter = new int[NN];    // but for real 3D problems this must be a big enough value!
   
   for (i=0; i<NN; i++) {     
      GtoL[i] = new int[noOfColGtoL];   
   }

   for(i=0; i<NN; i++) {
      for(j=0; j<noOfColGtoL; j++) {
		   GtoL[i][j] = -1;      // for the nodes that didn't connect 4 nodes
	   }
      GtoLCounter[i] = 0;
   }
   
   for (i=0; i<NE; i++) {
      for(j=0; j<NENv; j++) {
         GtoL[LtoG[i][j]][GtoLCounter[LtoG[i][j]]] = i;
         GtoLCounter[LtoG[i][j]] += 1;
      }
   }         
   
   delete [] GtoLCounter;
   // Su anda gereksiz 0'lar oluþturuluyor. Ýleride düzeltilebilir.
   // for(i=0; i<nVelNodes; i++) {   // extracting EBC values from GtoL with making them "-1"
      // for(j=0; j<noOfColGtoL; j++) {
         // GtoL[velNodes[i][0]][j] = -1;
      // }   
   // } 

// Finding size of col vector, creation of rowStarts & rowStartsSmall

   rowStarts = new int[Ndof+1];   // how many non zeros at rows of [K]
   rowStartsSmall = new int[NN];  // rowStarts for a piece of K(only for "u" velocity in another words 1/16 of the K(if NENv==NENp)) 
   checkCol = new int[1000];      // for checking the non zero column overlaps 
                                  // stores non-zero column number for rows (must be a large enough value)
   rowStarts[0] = 0;

   for(i=0; i<NN; i++) {
      NNZ = 0;
      
      if(GtoL[i][0] == -1) {
         NNZ = 1;
      } else {
      
         for(k=0; k<1000; k++) {      // prepare checkCol for new row
            checkCol[k] = -1;
         }
      
         for(j=0; j<noOfColGtoL; j++) {
            valGtoL = GtoL[i][j];
            if(valGtoL != -1) {
               for(x=0; x<NENp; x++) {
                  check = 1;         // for checking if column overlap occurs or not
                  for(y=0; y<NNZ; y++) {
                     if(checkCol[y] == (LtoG[valGtoL][x])) {   // this column was created
                        check = 0;
                     }
                  }
                  if (check) {
                     checkCol[NNZ]=(LtoG[valGtoL][x]);         // adding new non zero number to checkCol
                     NNZ++;
                  }
               }
            }   
         }
         
      }
      
      rowStarts[i+1] = NNZ + rowStarts[i];
   }

   // Creation of rowStarts from rowStarsSmall
   for (i=0; i<=NN; i++) {
      rowStartsSmall[i] = rowStarts[i];
   }  
 
   for (i=1; i<=NN; i++) {
      rowStarts[i] = rowStarts[i]*4;
   } 
   
   for (i=1; i<=NN*3; i++) {
      rowStarts[NN+i] = rowStarts[NN] + rowStarts[i];
   }
   
   delete [] checkCol;

   // col & colSmall creation

   col = new int[rowStarts[Ndof]];   // stores which non zero columns at which row data
   colSmall = new int[rowStarts[NN]/4]; // col for a piece of K(only for "u" velocity in another words 1/16 of the K(if NENv==NENp)) 

   for(i=0; i<NN; i++) {
      NNZ = 0;

      if(GtoL[i][0] == -1) {
         colSmall[rowStartsSmall[i]] = i;
      } else {
      
         for(j=0; j<noOfColGtoL; j++) {
            valGtoL = GtoL[i][j];
            if(valGtoL != -1) {
               for(x=0; x<NENp; x++) {
                  check = 1;
                  for(y=0; y<NNZ; y++) {
                     if(colSmall[rowStartsSmall[i]+y] == (LtoG[valGtoL][x])) {   // for checking if column overlap occurs or not
                        check = 0;
                     }
                  }
                  if (check) {
                     colSmall[rowStartsSmall[i]+NNZ] = (LtoG[valGtoL][x]);
                     NNZ++;
                  }
               }
            }   
         }
         
         for(k=1; k<NNZ; k++) {           // sorting the column vector values
            for(m=1; m<NNZ; m++) {        // for each row from smaller to bigger
               if(colSmall[rowStartsSmall[i]+m] < colSmall[rowStartsSmall[i]+m-1]) {
                  temp = colSmall[rowStartsSmall[i]+m];
                  colSmall[rowStartsSmall[i]+m] = colSmall[rowStartsSmall[i]+m-1];
                  colSmall[rowStartsSmall[i]+m-1] = temp;
               }
            }   
         }

      }      
   }
   
   // creation of col from colSmall
   m=0;
   for (i=0; i<NN; i++) {
      for (k=0; k<(4*NN); k=k+NN) {
         for (j=rowStartsSmall[i]; j<rowStartsSmall[i+1]; j++) {
            col[m]= colSmall[j] + k;
            m++;
         }
      }  
   }   
   
   for (i=0; i<rowStarts[NN]*3; i++) {
      col[i+rowStarts[NN]]=col[i];
   }


   ////----------------------CONTROL---------------------------------------

   //cout<<endl;
   //for (i=0; i<5; i++) { 
   //   for (j=rowStartsSmall[i]; j<rowStartsSmall[i+1]; j++) {
   //      cout<< colSmall[j] << " " ;   
   //   }
   //   cout<<endl;
   //}
   //cout<<endl;
   //cout<<endl;
   //for (i=0; i<5; i++) { 
   //   for (j=rowStarts[i]; j<rowStarts[i+1]; j++) {
   //      cout<< col[j] << " " ;   
   //   }
   //   cout<<endl;
   //}
   ////----------------------CONTROL---------------------------------------


   NNZ = rowStarts[Ndof];

   val = new real[NNZ];

   for(i=0; i<rowStarts[Ndof]; i++) {
      val[i] = 0;
   }

   for (i = 0; i<NN; i++) {   //deleting the unnecessary arrays for future
      delete GtoL[i];
   }
   delete [] GtoL;

} // End of function compressedSparseRowStorage()




//------------------------------------------------------------------------------
void gaussQuad()
//------------------------------------------------------------------------------
{
   // Generates the NGP-point Gauss quadrature points and weights.

   GQpoint = new double*[NGP];
   for (int i=0; i<NGP; i++) {
      GQpoint[i] = new double[3];     
   }
   GQweight = new double[NGP];

   if (eType == 3) {    // 3D Hexahedron element
   
      GQpoint[0][0] = -sqrt(1.0/3);   GQpoint[0][1] = -sqrt(1.0/3);  GQpoint[0][2] = -sqrt(1.0/3); 
      GQpoint[1][0] = sqrt(1.0/3);    GQpoint[1][1] = -sqrt(1.0/3);  GQpoint[1][2] = -sqrt(1.0/3);
      GQpoint[2][0] = sqrt(1.0/3);    GQpoint[2][1] = sqrt(1.0/3);   GQpoint[2][2] = -sqrt(1.0/3); 
      GQpoint[3][0] = -sqrt(1.0/3);   GQpoint[3][1] = sqrt(1.0/3);   GQpoint[3][2] = -sqrt(1.0/3); 
      GQpoint[4][0] = -sqrt(1.0/3);   GQpoint[4][1] = -sqrt(1.0/3);  GQpoint[4][2] = sqrt(1.0/3); 
      GQpoint[5][0] = sqrt(1.0/3);    GQpoint[5][1] = -sqrt(1.0/3);  GQpoint[5][2] = sqrt(1.0/3);
      GQpoint[6][0] = sqrt(1.0/3);    GQpoint[6][1] = sqrt(1.0/3);   GQpoint[6][2] = sqrt(1.0/3); 
      GQpoint[7][0] = -sqrt(1.0/3);   GQpoint[7][1] = sqrt(1.0/3);   GQpoint[7][2] = sqrt(1.0/3);      
      GQweight[0] = 1.0;
      GQweight[1] = 1.0;
      GQweight[2] = 1.0;
      GQweight[3] = 1.0;
      GQweight[4] = 1.0;
      GQweight[5] = 1.0;
      GQweight[6] = 1.0;
      GQweight[7] = 1.0;
       
   }
   else if (eType == 1) {       // Quadrilateral element  3 ile 4 ün yeri deðiþmeyecek mi sor!
      if (NGP == 1) {      // One-point quadrature
         GQpoint[0][0] = 0.0;            GQpoint[0][1] = 0.0;
         GQweight[0] = 4.0;
      } else if (NGP == 4) {  // Four-point quadrature
         GQpoint[0][0] = -sqrt(1.0/3);   GQpoint[0][1] = -sqrt(1.0/3);
         GQpoint[1][0] = sqrt(1.0/3);    GQpoint[1][1] = -sqrt(1.0/3);
         GQpoint[2][0] = -sqrt(1.0/3);   GQpoint[2][1] = sqrt(1.0/3);
         GQpoint[3][0] = sqrt(1.0/3);    GQpoint[3][1] = sqrt(1.0/3);
         GQweight[0] = 1.0;
         GQweight[1] = 1.0;
         GQweight[2] = 1.0;
         GQweight[3] = 1.0;
      } else if (NGP == 9) {  // Nine-point quadrature
         GQpoint[0][0] = -sqrt(3.0/5);  GQpoint[0][1] = -sqrt(3.0/5);
         GQpoint[1][0] = 0.0;           GQpoint[1][1] = -sqrt(3.0/5);
         GQpoint[2][0] = sqrt(3.0/5);   GQpoint[2][1] = -sqrt(3.0/5);
         GQpoint[3][0] = -sqrt(3.0/5);  GQpoint[3][1] = 0.0;
         GQpoint[4][0] = 0.0;           GQpoint[4][1] = 0.0;
         GQpoint[5][0] = sqrt(3.0/5);   GQpoint[5][1] = 0.0;
         GQpoint[6][0] = -sqrt(3.0/5);  GQpoint[6][1] = sqrt(3.0/5);
         GQpoint[7][0] = 0.0;           GQpoint[7][1] = sqrt(3.0/5);
         GQpoint[8][0] = sqrt(3.0/5);   GQpoint[8][1] = sqrt(3.0/5);

         GQweight[0] = 5.0/9 * 5.0/9;
         GQweight[1] = 8.0/9 * 5.0/9;
         GQweight[2] = 5.0/9 * 5.0/9;
         GQweight[3] = 5.0/9 * 8.0/9;
         GQweight[4] = 8.0/9 * 8.0/9;
         GQweight[5] = 5.0/9 * 8.0/9;
         GQweight[6] = 5.0/9 * 5.0/9;
         GQweight[7] = 8.0/9 * 5.0/9;
         GQweight[8] = 5.0/9 * 5.0/9;
      } else if (NGP == 16) { // Sixteen-point quadrature
         GQpoint[0][0] = -0.8611363116;   GQpoint[0][1] = -0.8611363116;
         GQpoint[1][0] = -0.3399810435;   GQpoint[1][1] = -0.8611363116;
         GQpoint[2][0] =  0.3399810435;   GQpoint[2][1] = -0.8611363116;
         GQpoint[3][0] =  0.8611363116;   GQpoint[3][1] = -0.8611363116;
         GQpoint[4][0] = -0.8611363116;   GQpoint[4][1] = -0.3399810435;
         GQpoint[5][0] = -0.3399810435;   GQpoint[5][1] = -0.3399810435;
         GQpoint[6][0] =  0.3399810435;   GQpoint[6][1] = -0.3399810435;
         GQpoint[7][0] =  0.8611363116;   GQpoint[7][1] = -0.3399810435;
         GQpoint[8][0] = -0.8611363116;   GQpoint[8][1] =  0.3399810435;
         GQpoint[9][0] = -0.3399810435;   GQpoint[9][1] =  0.3399810435;
         GQpoint[10][0]=  0.3399810435;   GQpoint[10][1]=  0.3399810435;
         GQpoint[11][0]=  0.8611363116;   GQpoint[11][1]=  0.3399810435;
         GQpoint[12][0]= -0.8611363116;   GQpoint[12][1]=  0.8611363116;
         GQpoint[13][0]= -0.3399810435;   GQpoint[13][1]=  0.8611363116;
         GQpoint[14][0]=  0.3399810435;   GQpoint[14][1]=  0.8611363116;
         GQpoint[15][0]=  0.8611363116;   GQpoint[15][1]=  0.8611363116;

         GQweight[0] = 0.3478548451 * 0.3478548451;
         GQweight[1] = 0.3478548451 * 0.6521451548;
         GQweight[2] = 0.3478548451 * 0.6521451548;
         GQweight[3] = 0.3478548451 * 0.3478548451;
         GQweight[4] = 0.6521451548 * 0.3478548451;
         GQweight[5] = 0.6521451548 * 0.6521451548;
         GQweight[6] = 0.6521451548 * 0.6521451548;
         GQweight[7] = 0.6521451548 * 0.3478548451;
         GQweight[8] = 0.6521451548 * 0.3478548451;
         GQweight[9] = 0.6521451548 * 0.6521451548;
         GQweight[10] = 0.6521451548 * 0.6521451548;
         GQweight[11] = 0.6521451548 * 0.3478548451;
         GQweight[12] = 0.3478548451 * 0.3478548451;
         GQweight[13] = 0.3478548451 * 0.6521451548;
         GQweight[14] = 0.3478548451 * 0.6521451548;
         GQweight[15] = 0.3478548451 * 0.3478548451;
      }
   } else if (eType == 2) {  // Triangular element
      if (NGP == 1) {          // One-point quadrature
         GQpoint[0][0] = 1.0/3;  GQpoint[0][1] = 1.0/3;
         GQweight[0] = 0.5;
      } else if (NGP == 3) {   // Two-point quadrature
         GQpoint[0][0] = 0.5;   GQpoint[0][1] = 0.0;
         GQpoint[1][0] = 0.0;   GQpoint[1][1] = 0.5;
         GQpoint[2][0] = 0.5;   GQpoint[2][1] = 0.5;

         GQweight[0] = 1.0/6;
         GQweight[1] = 1.0/6;
         GQweight[2] = 1.0/6;
      } else if (NGP == 4) {   // Four-point quadrature
         GQpoint[0][0] = 1.0/3;   GQpoint[0][1] = 1.0/3;
         GQpoint[1][0] = 0.6;     GQpoint[1][1] = 0.2;
         GQpoint[2][0] = 0.2;     GQpoint[2][1] = 0.6;
         GQpoint[3][0] = 0.2;     GQpoint[3][1] = 0.2;

         GQweight[0] = -27.0/96;
         GQweight[1] = 25.0/96;
         GQweight[2] = 25.0/96;
         GQweight[3] = 25.0/96;
      } else if (NGP == 7) {  // Seven-point quadrature
         GQpoint[0][0] = 1.0/3;               GQpoint[0][1] = 1.0/3;
         GQpoint[1][0] = 0.059715871789770;   GQpoint[1][1] = 0.470142064105115;
         GQpoint[2][0] = 0.470142064105115;   GQpoint[2][1] = 0.059715871789770;
         GQpoint[3][0] = 0.470142064105115;   GQpoint[3][1] = 0.470142064105115;
         GQpoint[4][0] = 0.101286507323456;   GQpoint[4][1] = 0.797426985353087;
         GQpoint[5][0] = 0.101286507323456;   GQpoint[5][1] = 0.101286507323456;
         GQpoint[6][0] = 0.797426985353087;   GQpoint[6][1] = 0.101286507323456;

         GQweight[0] = 0.225 / 2;
         GQweight[1] = 0.132394152788 / 2;
         GQweight[2] = 0.132394152788 / 2;
         GQweight[3] = 0.132394152788 / 2;
         GQweight[4] = 0.125939180544 / 2;
         GQweight[5] = 0.125939180544 / 2;
         GQweight[6] = 0.125939180544 / 2;
      }
   }

} // End of function gaussQuad()




//------------------------------------------------------------------------------
void calcShape()
//------------------------------------------------------------------------------
{
   // Calculates the values of the shape functions and their derivatives with
   // respect to ksi and eta at GQ points.

   double ksi, eta, zeta;
   
   if (eType == 3) { // 3D Hexahedron Element
      if (NENp == 8) {
      
         Sp = new double*[8];
         for (int i=0; i<NGP; i++) {
            Sp[i] = new double[NGP];
         }
         
         DSp = new double **[3];	
         for (int i=0; i<3; i++) {
            DSp[i] = new double *[8];
            for (int j=0; j<8; j++) {
               DSp[i][j] = new double[NGP];
            }
         }     
         
         for (int k = 0; k<NGP; k++) {
            ksi = GQpoint[k][0];
            eta = GQpoint[k][1];
            zeta = GQpoint[k][2];
            
            Sp[0][k] = 0.25*(1-ksi)*(1-eta)*(1-zeta);
            Sp[1][k] = 0.25*(1+ksi)*(1-eta)*(1-zeta);
            Sp[2][k] = 0.25*(1+ksi)*(1+eta)*(1-zeta);
            Sp[3][k] = 0.25*(1-ksi)*(1+eta)*(1-zeta);   
            Sp[4][k] = 0.25*(1-ksi)*(1-eta)*(1+zeta);
            Sp[5][k] = 0.25*(1+ksi)*(1-eta)*(1+zeta);
            Sp[6][k] = 0.25*(1+ksi)*(1+eta)*(1+zeta);
            Sp[7][k] = 0.25*(1-ksi)*(1+eta)*(1+zeta); 

            DSp[0][0][k] = -0.25*(1-eta)*(1-zeta);  // ksi derivative of the 1st shape funct. at k-th GQ point.
            DSp[1][0][k] = -0.25*(1-ksi)*(1-zeta);  // eta derivative of the 1st shape funct. at k-th GQ point.  
            DSp[2][0][k] = -0.25*(1-ksi)*(1-eta);   // zeta derivative of the 1st shape funct. at k-th GQ point.
            DSp[0][1][k] =  0.25*(1-eta)*(1-zeta);   
            DSp[1][1][k] = -0.25*(1+ksi)*(1-zeta);
            DSp[2][1][k] = -0.25*(1+ksi)*(1-eta);         
            DSp[0][2][k] =  0.25*(1+eta)*(1-zeta);   
            DSp[1][2][k] =  0.25*(1+ksi)*(1-zeta);
            DSp[2][2][k] = -0.25*(1+ksi)*(1+eta);         
            DSp[0][3][k] = -0.25*(1+eta)*(1-zeta);   
            DSp[1][3][k] =  0.25*(1-ksi)*(1-zeta);
            DSp[2][3][k] = -0.25*(1-ksi)*(1+eta); 
            DSp[0][4][k] = -0.25*(1-eta)*(1+zeta);  // ksi derivative of the 1st shape funct. at k-th GQ point.
            DSp[1][4][k] = -0.25*(1-ksi)*(1+zeta);  // eta derivative of the 1st shape funct. at k-th GQ point.  
            DSp[2][4][k] = 0.25*(1-ksi)*(1-eta);   // zeta derivative of the 1st shape funct. at k-th GQ point.
            DSp[0][5][k] =  0.25*(1-eta)*(1+zeta);   
            DSp[1][5][k] = -0.25*(1+ksi)*(1+zeta);
            DSp[2][5][k] = 0.25*(1+ksi)*(1-eta);         
            DSp[0][6][k] =  0.25*(1+eta)*(1+zeta);   
            DSp[1][6][k] =  0.25*(1+ksi)*(1+zeta);
            DSp[2][6][k] = 0.25*(1+ksi)*(1+eta);         
            DSp[0][7][k] = -0.25*(1+eta)*(1+zeta);   
            DSp[1][7][k] =  0.25*(1-ksi)*(1+zeta);
            DSp[2][7][k] = 0.25*(1-ksi)*(1+eta); 
         }
      }
      
      if (NENv == 8) {
      
         Sv = new double*[8];
         for (int i=0; i<NGP; i++) {
            Sv[i] = new double[NGP];
         }
      
         DSv = new double **[3];	
         for (int i=0; i<3; i++) {
            DSv[i] = new double *[8];
            for (int j=0; j<8; j++) {
               DSv[i][j] = new double[NGP];
            }
         }
         
         for (int i=0; i<NGP; i++) {
            Sv[i] = Sp[i];
         }
         
         for (int i=0; i<3; i++) {
            for (int j=0; j<8; j++) {
               DSv[i][j] = DSp[i][j];
            }
         }         
               
      }
   }   
      
   for (int i = 0; i<8; i++) {   //deleting the unnecessary arrays for future
      delete GQpoint[i];
   }
   delete [] GQpoint;
      
} // End of function calcShape()




//------------------------------------------------------------------------------
void calcJacobian()
//------------------------------------------------------------------------------
{
   // Calculates the Jacobian matrix, its inverse and determinant for all
   // elements at all GQ points. Also evaluates and stores derivatives of shape
   // functions wrt global coordinates x and y.

   int e, i, j, k, x, iG; 
   double **e_coord;
   double **Jacob, **invJacob;
   double temp;
   
   if (eType == 3) {
   
      e_coord = new double*[NENp];
   
      for (i=0; i<NENp; i++) {
         e_coord[i] = new double[3];
      }
   
      Jacob = new double*[3];
      invJacob = new double*[3];
      for (i=0; i<3; i++) {
         Jacob[i] = new double[3];
         invJacob[i] = new double[3];
      }
      
      detJacob = new double*[NE];
      for (i=0; i<NE; i++) {
         detJacob[i] = new double[NGP];
      }
      
      gDSp = new double***[NE];

      for (i=0; i<NE; i++) {
         gDSp[i] = new double**[3];
         for(j=0; j<3; j++) {
            gDSp[i][j] = new double*[NENv];
            for(k=0; k<NENp; k++) {
               gDSp[i][j][k] = new double[NGP];
            }
         }	
      }
      
      gDSv = new double***[NE];

      for (i=0; i<NE; i++) {
         gDSv[i] = new double**[3];
         for(j=0; j<3; j++) {
            gDSv[i][j] = new double*[NENv];
            for(k=0; k<NENv; k++) {
               gDSv[i][j][k] = new double[NGP];
            }
         }	
      }      
   
      for (e = 0; e<NE; e++){
         // To calculate Jacobian matrix of an element we need e_coord matrix of
         // size NEN*2. Each row of it stores x, y & z coordinates of the nodes of
         // an element.
         for (i = 0; i<NENp; i++){
            iG = LtoG[e][i];
            e_coord[i][0] = coord[iG][0]; 
            e_coord[i][1] = coord[iG][1];
            e_coord[i][2] = coord[iG][2];
         }
      
         // For each GQ point calculate 3*3 Jacobian matrix, its inverse and its
         // determinant. Also calculate derivatives of shape functions wrt global
         // coordinates x and y & z. These are the derivatives that we'll use in
         // evaluating K and F integrals. 
   	
         for (k = 0; k<NGP; k++) {
            for (i = 0; i<3; i++) {
               for (j = 0; j<3; j++) {
                  temp = 0;
                  for (x = 0; x<NENp; x++) {
                     temp = DSp[i][x][k] * e_coord[x][j]+temp;
                  }
                  Jacob[i][j] = temp;
               }
            }
            
            invJacob[0][0] = Jacob[1][1]*Jacob[2][2]-Jacob[2][1]*Jacob[1][2];
            invJacob[0][1] = -(Jacob[0][1]*Jacob[2][2]-Jacob[0][2]*Jacob[2][1]);
            invJacob[0][2] = Jacob[0][1]*Jacob[1][2]-Jacob[1][1]*Jacob[0][2];
            invJacob[1][0] =  -(Jacob[1][0]*Jacob[2][2]-Jacob[1][2]*Jacob[2][0]);
            invJacob[1][1] = Jacob[2][2]*Jacob[0][0]-Jacob[2][0]*Jacob[0][2];
            invJacob[1][2] = -(Jacob[1][2]*Jacob[0][0]-Jacob[1][0]*Jacob[0][2]);
            invJacob[2][0] = Jacob[1][0]*Jacob[2][1]-Jacob[2][0]*Jacob[1][1];
            invJacob[2][1] = -(Jacob[2][1]*Jacob[0][0]-Jacob[2][0]*Jacob[0][1]);
            invJacob[2][2] = Jacob[1][1]*Jacob[0][0]-Jacob[1][0]*Jacob[0][1];

            detJacob[e][k] = Jacob[0][0]*(Jacob[1][1]*Jacob[2][2]-Jacob[2][1]*Jacob[1][2]) +
                            Jacob[0][1]*(Jacob[1][2]*Jacob[2][0]-Jacob[1][0]*Jacob[2][2]) +
                            Jacob[0][2]*(Jacob[1][0]*Jacob[2][1]-Jacob[1][1]*Jacob[2][0]);
            
            for (i = 0; i<3; i++){
               for (j = 0; j<3; j++){
                  invJacob[i][j] = invJacob[i][j]/detJacob[e][k];
               }    
            }
         
            for (i = 0; i<3; i++){
               for (j = 0; j<8; j++){
                  temp = 0;
                  for (x = 0; x<3; x++){
                     temp = invJacob[i][x] * DSp[x][j][k]+temp;
                  }
                  gDSp[e][i][j][k] = temp;
                  gDSv[e][i][j][k] = gDSp[e][i][j][k];
               }
            }
         }
      }
   } 
   
}  // End of function calcJacobian()




//------------------------------------------------------------------------------
void calcGlobalSys()
//------------------------------------------------------------------------------
{
   // Calculates Ke and Fe one by one for each element and assembles them into
   // the global K and F.

   int e, i, j, k, m, n;
   double Tau;
   //   double x, y, axy, fxy;
   double *Fe, **Ke;
   double *Fe_1, *Fe_2, *Fe_3, *Fe_4;
   double **Ke_11, **Ke_12, **Ke_13, **Ke_14, **Ke_22, **Ke_23, **Ke_24, **Ke_33, **Ke_34, **Ke_44;
   double **Ke_11_add, **Ke_12_add, **Ke_13_add, **Ke_14_add, **Ke_22_add, **Ke_23_add, **Ke_24_add, **Ke_33_add, **Ke_34_add, **Ke_44_add;


   // Initialize the arrays
   F = new real[Ndof];
   K = new real*[Ndof];	
   for (i=0; i<Ndof; i++) {
      K[i] = new real[Ndof];
   }

   for (i=0; i<Ndof; i++) {
      F[i] = 0;
      for (j=0; j<Ndof; j++) {
         K[i][j] = 0;
	   }
   }

   Fe = new double[NEU];
   Ke = new double*[NEU];	
   for (i=0; i<NEU; i++) {
      Ke[i] = new double[NEU];
   }
   
   Fe_1 = new double[NENv];
   Fe_2 = new double[NENv];
   Fe_3 = new double[NENv];
   Fe_4 = new double[NENp];
   
   Ke_11 = new double*[NENv];
   Ke_12 = new double*[NENv];
	Ke_13 = new double*[NENv];
   Ke_14 = new double*[NENv];
   // Ke_21 is the transpose of Ke_12
   Ke_22 = new double*[NENv];
   Ke_23 = new double*[NENv];
	Ke_24 = new double*[NENv]; 
   // Ke_31 is the transpose of Ke_13
   // Ke_32 is the transpose of Ke_23
	Ke_33 = new double*[NENv];
	Ke_34 = new double*[NENv];
   // Ke_41 is the transpose of Ke_14
   // Ke_42 is the transpose of Ke_24
   // Ke_43 is the transpose of Ke_34
	Ke_44 = new double*[NENp];
   
   Ke_11_add = new double*[NENv];
   Ke_12_add = new double*[NENv];
   Ke_13_add = new double*[NENv];
   Ke_14_add = new double*[NENv];
   Ke_22_add = new double*[NENv];
   Ke_23_add = new double*[NENv];
   Ke_24_add = new double*[NENv];
   Ke_33_add = new double*[NENv];
   Ke_34_add = new double*[NENv];
   Ke_44_add = new double*[NENp];
   for (i=0; i<NENv; i++) {
      Ke_11[i] = new double[NENv];
      Ke_12[i] = new double[NENv];
      Ke_13[i] = new double[NENv];
      Ke_14[i] = new double[NENp];
      Ke_22[i] = new double[NENv];
      Ke_23[i] = new double[NENv];
      Ke_24[i] = new double[NENp];
      Ke_33[i] = new double[NENv];
      Ke_34[i] = new double[NENp];
      
      Ke_11_add[i] = new double[NENv];
      Ke_12_add[i] = new double[NENv];
      Ke_13_add[i] = new double[NENv];
      Ke_14_add[i] = new double[NENp];
      Ke_22_add[i] = new double[NENv];
      Ke_23_add[i] = new double[NENv];
      Ke_24_add[i] = new double[NENp];
      Ke_33_add[i] = new double[NENv];
      Ke_34_add[i] = new double[NENp];
   }
   for (i=0; i<NENp; i++) {   
      Ke_44[i] = new double[NENp];   
      Ke_44_add[i] = new double[NENp];
   }

   // Calculating the elemental stiffness matrix(Ke) and force vector(Fe)

   for (e = 0; e<NE; e++) {
      // Intitialize Ke and Fe to zero.
      for (i=0; i<NEU; i++) {
         Fe[i] = 0;
         for (j=0; j<NEU; j++) {
            Ke[i][j] = 0;
         }
      }

      for (i=0; i<NENv; i++) {
         Fe_1[i] = 0;
         Fe_2[i] = 0;
         Fe_3[i] = 0;
         Fe_4[i] = 0;
         for (j=0; j<NENv; j++) {
            Ke_11[i][j] = 0;
            Ke_12[i][j] = 0;
            Ke_13[i][j] = 0;
            Ke_22[i][j] = 0;
            Ke_23[i][j] = 0;
            Ke_33[i][j] = 0;
         }
         for (j=0; j<NENp; j++) {
            Ke_14[i][j] = 0;
            Ke_24[i][j] = 0;
            Ke_34[i][j] = 0;
         }         
      }   
      for (i=0; i<NENp; i++) {
         for (j=0; j<NENp; j++) {
            Ke_44[i][j] = 0;
         }
      }
      
      for (k = 0; k<NGP; k++) {   // Gauss quadrature loop
      
         for (i=0; i<NENv; i++) {
            for (j=0; j<NENv; j++) {
               Ke_11_add[i][j] = 0;
               Ke_12_add[i][j] = 0;
               Ke_13_add[i][j] = 0;
               Ke_22_add[i][j] = 0;
               Ke_23_add[i][j] = 0;
               Ke_33_add[i][j] = 0;
            }
            for (j=0; j<NENp; j++) {
               Ke_14_add[i][j] = 0;
               Ke_24_add[i][j] = 0;
               Ke_34_add[i][j] = 0;
            }         
         }
         for (i=0; i<NENp; i++) {
            for (j=0; j<NENp; j++) {
               Ke_44_add[i][j] = 0;
            }
         }

         for (i=0; i<NENv; i++) {
            for (j=0; j<NENv; j++) {
               Ke_11_add[i][j] = Ke_11_add[i][j] + viscosity * (
                     2 * gDSv[e][0][i][k] * gDSv[e][0][j][k] + 
                     gDSv[e][1][i][k] * gDSv[e][1][j][k] +
                     gDSv[e][2][i][k] * gDSv[e][2][j][k]) ;
                     //density * Sv[i][k];
                     
               Ke_12_add[i][j] = Ke_12_add[i][j] + viscosity * gDSv[e][1][i][k] * gDSv[e][0][j][k];
               
               Ke_13_add[i][j] = Ke_13_add[i][j] + viscosity * gDSv[e][2][i][k] * gDSv[e][0][j][k];
               
               Ke_22_add[i][j] = Ke_22_add[i][j] + viscosity * (
                     gDSv[e][0][i][k] * gDSv[e][0][j][k] + 
                     2 * gDSv[e][1][i][k] * gDSv[e][1][j][k] +
                     gDSv[e][2][i][k] * gDSv[e][2][j][k]) ;
                     // density * Sv[i][k];
                     
               Ke_23_add[i][j] = Ke_23_add[i][j] + viscosity * gDSv[e][2][i][k] * gDSv[e][1][j][k];
               
               Ke_33_add[i][j] = Ke_33_add[i][j] + viscosity * (
                     gDSv[e][0][i][k] * gDSv[e][0][j][k] + 
                     gDSv[e][1][i][k] * gDSv[e][1][j][k] +
                     2 * gDSv[e][2][i][k] * gDSv[e][2][j][k]) ;
                     // density * Sv[i][k];
                     
            }
            for (j=0; j<NENp; j++) {
               Ke_14_add[i][j] = Ke_14_add[i][j] - gDSv[e][0][i][k] * Sp[j][k];
               Ke_24_add[i][j] = Ke_24_add[i][j] - gDSv[e][1][i][k] * Sp[j][k];
               Ke_34_add[i][j] = Ke_34_add[i][j] - gDSv[e][2][i][k] * Sp[j][k];
            }         
         }

  // Apply GLS stabilization for linear elements with NENv = NENp
    
         Tau = ((1.0/12.0)*2) / viscosity;  // GLS parameter

         for (i=0; i<NENp; i++) {
            for (j=0; j<NENp; j++) {
               Ke_44_add[i][j] = Ke_44_add[i][j] - Tau * ( gDSv[e][0][i][k] * gDSv[e][0][j][k] + gDSv[e][1][i][k] * gDSv[e][1][j][k] + gDSv[e][2][i][k] * gDSv[e][2][j][k] );
               Ke_44_add[i][j] = Ke_44_add[i][j] - Tau * ( gDSv[e][0][i][k] * gDSv[e][0][j][k] + gDSv[e][1][i][k] * gDSv[e][1][j][k] + gDSv[e][2][i][k] * gDSv[e][2][j][k] );
            }
         }

  // Apply GLS stabilization for linear elements with NENv = NENp
            
         for (i=0; i<NENv; i++) {
            for (j=0; j<NENv; j++) {            
               Ke_11[i][j] += Ke_11_add[i][j] * detJacob[e][k] * GQweight[k];
               Ke_12[i][j] += Ke_12_add[i][j] * detJacob[e][k] * GQweight[k];
               Ke_13[i][j] += Ke_13_add[i][j] * detJacob[e][k] * GQweight[k];
               Ke_22[i][j] += Ke_22_add[i][j] * detJacob[e][k] * GQweight[k];
               Ke_23[i][j] += Ke_23_add[i][j] * detJacob[e][k] * GQweight[k];
               Ke_33[i][j] += Ke_33_add[i][j] * detJacob[e][k] * GQweight[k];
            }
            for (j=0; j<NENp; j++) {               
               Ke_14[i][j] += Ke_14_add[i][j] * detJacob[e][k] * GQweight[k];
               Ke_24[i][j] += Ke_24_add[i][j] * detJacob[e][k] * GQweight[k];
               Ke_34[i][j] += Ke_34_add[i][j] * detJacob[e][k] * GQweight[k];
            }
         }
         for (i=0; i<NENv; i++) {
            for (j=0; j<NENv; j++) {
               Ke_44[i][j] += Ke_44_add[i][j] * detJacob[e][k] * GQweight[k];
            }
         }

      }   // End GQ loop  


      // Assembly of Fe

      i=0;
      for (j=0; j<NENv; j++) {
         Fe[i]=Fe_1[j];
         i++;
      }
      for (j=0; j<NENv; j++) {
         Fe[i]=Fe_2[j];
         i++;
      }
      for (j=0; j<NENv; j++) {
         Fe[i]=Fe_3[j];
         i++;
      }
      for (j=0; j<NENp; j++) {
         Fe[i]=Fe_4[j];
         i++;
      }         


      // Assembly of Ke

      i=0;
      j=0;    
      for (m=0; m<NENv; m++) {
         j=0;
         for (n=0; n<NENv; n++) {       
            Ke[i][j] =  Ke_11[m][n];
            j++;
         }
         i++;
      }
      
      i=i-NENv;
      for (m=0; m<NENv; m++) {
         j=0;
         for (n=0; n<NENv; n++) {       
            Ke[i][j+NENv] =  Ke_12[m][n];
            j++;
         }
         i++;
      }

      i=i-NENv;
      for (m=0; m<NENv; m++) {
         j=0;
         for (n=0; n<NENv; n++) {       
            Ke[i][j+2*NENv] =  Ke_13[m][n];
            j++;
         }
         i++;
      }
      
      i=i-NENv;
      for (m=0; m<NENv; m++) {
         j=0;
         for (n=0; n<NENp; n++) {       
            Ke[i][j+3*NENv] =  Ke_14[m][n];
            j++;
         }
         i++;
      }
      
      for (m=0; m<NENv; m++) {
         j=0;
         for (n=0; n<NENv; n++) {  
            Ke[i][j] =  Ke_12[n][m];
            j++;
         }
         i++;
      }

      i=i-NENv;
      for (m=0; m<NENv; m++) {
         j=0;
         for (n=0; n<NENv; n++) {       
            Ke[i][j+NENv] =  Ke_22[m][n];
            j++;
         }
         i++;
      }

      i=i-NENv;
      for (m=0; m<NENv; m++) {
         j=0;
         for (n=0; n<NENv; n++) {       
            Ke[i][j+2*NENv] =  Ke_23[m][n];
            j++;
         }
         i++;
      }

      i=i-NENv;
      for (m=0; m<NENv; m++) {
         j=0;
         for (n=0; n<NENp; n++) {       
            Ke[i][j+3*NENv] =  Ke_24[m][n];
            j++;
         }
         i++;
      }

      for (m=0; m<NENv; m++) {
         j=0;
         for (n=0; n<NENv; n++) {       
            Ke[i][j] =  Ke_13[n][m];
            j++;
         }
         i++;
      }      

      i=i-NENv;
      for (m=0; m<NENv; m++) {
         j=0;
         for (n=0; n<NENv; n++) {       
            Ke[i][j+NENv] =  Ke_23[n][m];
            j++;
         }
         i++;
      } 

      i=i-NENv;
      for (m=0; m<NENv; m++) {
         j=0;
         for (n=0; n<NENv; n++) {       
            Ke[i][j+2*NENv] =  Ke_33[m][n];
            j++;
         }
         i++;
      }

      i=i-NENv;
      for (m=0; m<NENv; m++) {
         j=0;
         for (n=0; n<NENp; n++) {       
            Ke[i][j+3*NENv] =  Ke_34[m][n];
            j++;
         }
         i++;
      }

      j=0;
      for (m=0; m<NENv; m++) {
         j=0;
         for (n=0; n<NENp; n++) {       
            Ke[i][j] =  Ke_14[n][m];
            j++;
         }
         i++;
      }
      
      i=i-NENv;
      for (m=0; m<NENv; m++) {
         j=0;
         for (n=0; n<NENp; n++) {       
            Ke[i][j+NENv] =  Ke_24[n][m];
            j++;
         }
         i++;
      } 

      i=i-NENv;
      for (m=0; m<NENv; m++) {
         j=0;
         for (n=0; n<NENp; n++) {       
            Ke[i][j+2*NENv] =  Ke_34[n][m];
            j++;
         }
         i++;
      } 

      i=i-NENv;
      for (m=0; m<NENp; m++) {
         j=0;
         for (n=0; n<NENp; n++) {       
            Ke[i][j+3*NENv] = Ke_44[m][n];
            j++;
         }
         i++;
      }

      assemble(e, Ke, Fe);  // sending Ke & Fe for assembly

   }   // End element loop
 
} // End of function calcGlobalSys()




//------------------------------------------------------------------------------
void assemble(int e, double **Ke, double *Fe)
//------------------------------------------------------------------------------
{
   // Inserts Ke and Fe into proper locations of K and F.

   int i, j, m, n, iG, jG; 
   
   // Inserts [Ke] and {Fe} into proper locations of [K] and {F} by the help of LtoG array. 
   
   for (i = 0; i<NEU; i++) {
         
      iG = LtoG[e][i%NENv] + NN*(i/NENv);
      F[iG] = F[iG] + Fe[i];
      
      for (j = 0; j<NEU; j++) {   
         jG = LtoG[e][j%NENv] + NN*(j/NENv);
         K[iG][jG] = K[iG][jG] + Ke[i][j];
      }
   }   

   // Assembly process for compressed sparse storage 
   // KeKMapSmall creation

   int shiftRow, shiftCol;
   int *nodeData, p, q, k;
   // int *nodeDataEBC;
    
   // nodeDataEBC = new int[NENv];     // Elemental node data(LtoG data) (modified with EBC)    // BC implementationu sonraya býrak ineff ama bir anda zor
   nodeData = new int[NENv];           // Stores sorted LtoG data
   for(k=0; k<NENv; k++) {
      nodeData[k] = (LtoG[e][k]);   //takes node data from LtoG
   } 

   // for(i=0; i<NEN; i++) {
      // nodeData[i]=nodeDataEBC[i];
      // if (GtoL[nodeDataEBC[i]][0] == -1) {
         // val[rowStarts[nodeDataEBC[i]]] = 1*bigNumber;      // Must be fixed, val[x] = 1 repeating for every element that contains the node!
         // nodeDataEBC[i] = -1;
      // }
   // }

   KeKMapSmall = new int*[NENv];        // NENv X NENv                                          
   for(j=0; j<NENv; j++) {              // Stores map data between K elemental and value vector
      KeKMapSmall[j] = new int[NENv];
   }

   for(i=0; i<NENv; i++) {
      for(j=0; j<NENv; j++) {
         q=0;
         for(p=rowStartsSmall[nodeData[i]]; p<rowStartsSmall[nodeData[i]+1]; p++) {  // p is the location of the col vector(col[x], p=x) 
            if(colSmall[p] == nodeData[j]) {                                         // Selection process of the KeKMapSmall data from the col vector
               KeKMapSmall[i][j] = q; 
               break;
            }
            q++;
         }
      }
   }


   // Creating val vector
   for(shiftRow=1; shiftRow<5; shiftRow++) {
      for(shiftCol=1; shiftCol<5; shiftCol++) {
         for(i=0; i<NENv; i++) {
            for(j=0; j<NENv; j++) {
               val[ rowStarts[LtoG[ e ][ i ] + NN * (shiftRow-1)] + ((rowStartsSmall[LtoG[ e ][ i ]+1]-
                  rowStartsSmall[LtoG[ e ][ i ]]) * (shiftCol-1)) + KeKMapSmall[i][j]] +=
                     Ke[ i + ((shiftRow - 1) * NENv) ][ j + ((shiftCol - 1) * NENv) ] ;
            }
         }
      }
   }         
} // End of function assemble()




//------------------------------------------------------------------------------
void applyBC()
//------------------------------------------------------------------------------
{
   // For EBCs reduction is not applied. Instead K and F are modified as
   // explained in class, which requires modification of both [K] and {F}.
   // SV values specified for NBCs are added to {F}.
   
   // Deleting the unnecessary arrays for future

   delete [] GQweight;
   
   for (int i=0; i<NGP; i++) {
      delete Sp[i];
   }
   delete [] Sp;
   
   //for (int i=0; i<NGP; i++) {
   //   delete Sv[i];
   //}
   //delete [] Sv;   

   for (int i=0; i<3; i++) {
      for (int j=0; j<8; j++) {
         delete DSp[i][j];
      }
   }
   for (int i=0; i<3; i++) {
      delete DSp[i];
   }
   delete [] DSp;

   //for (int i=0; i<3; i++) {
   //   for (int j=0; j<8; j++) {
   //      delete DSv[i][j];
   //   }
   //}
   //for (int i=0; i<3; i++) {
   //   delete DSv[i];
   //}
   //delete [] DSv;

   for (int i=0; i<NE; i++) {
      delete detJacob[i];
   }
   delete [] detJacob;

   for (int i=0; i<NE; i++) {
      for(int j=0; j<3; j++) {
         for(int k=0; k<NENp; k++) {
            delete gDSp[i][j][k];
         }
      }	
   }   
   for (int i=0; i<NE; i++) {
      for(int j=0; j<3; j++) {
         delete gDSp[i][j];
      }
   }      
   for (int i=0; i<NE; i++) {
      delete gDSp[i];
   }
   delete [] gDSp;

   for (int i=0; i<NE; i++) {
      for(int j=0; j<3; j++) {
         for(int k=0; k<NENv; k++) {
            delete gDSv[i][j][k];
         }
      }	
   }   
   for (int i=0; i<NE; i++) {
      for(int j=0; j<3; j++) {
         delete gDSv[i][j];
      }
   }      
   for (int i=0; i<NE; i++) {
      delete gDSv[i];
   }
   delete [] gDSv;   

   

   int i, j, whichBC, node ;
   double x, y, z; 

   bigNumber = 1000;            // To make the sparse matrix diagonally dominant


   // Modify [K] and {F} for velocity BCs. [FULL STORAGE]

   for (i = 0; i<nVelNodes; i++) {
      node = velNodes[i][0];         // Node at which this EBC is specified
   	
      x = coord[node][0];              // May be necessary for BCstring evaluation
      y = coord[node][1];
      z = coord[node][2];

      whichBC = velNodes[i][1]-1;      // Number of the specified BC
      
      F[node] = BCstrings[whichBC][0]*bigNumber;    // Specified value of the PV
      for (j=0; j<Ndof; j++) {
         K[node][j] = 0.0;
      }
      K[node][node] = 1.0*bigNumber;
      
      F[node + NN] = BCstrings[whichBC][1]*bigNumber;    // Specified value of the PV
      for (j=0; j<Ndof; j++) {
         K[node + NN][j] = 0.0;
      }
      K[node + NN][node + NN] = 1.0*bigNumber;
      
      F[node + NN*2] = BCstrings[whichBC][2]*bigNumber;    // Specified value of the PV
      for (j=0; j<Ndof; j++) {
         K[node + NN*2][j] = 0.0;
      }
      K[node + NN*2][node + NN*2] = 1.0*bigNumber;      
   }

   // Modify [K] and {F} for pressure BCs.  
   for (i = 0; i<nPressureNodes; i++) {
      node = pressureNodes[i][0];         // Node at which this EBC is specified
   	
      x = coord[node][0];                // May be necessary for BCstring evaluation
      y = coord[node][1];
      z = coord[node][2];

      whichBC = pressureNodes[i][1]-1;      // Number of the specified BC   	
      
      F[node + NN*3] = BCstrings[whichBC][0]*bigNumber;    // Specified value of the PV
      for (j=0; j<Ndof; j++) {
         K[node + NN*3][j] = 0.0;
      }
      K[node + NN*3][node + NN*3] = 1.0*bigNumber;          
   }

    
   // Modify CSR vectors for BCs [CSR STORAGE]
   
   int p, q; 

   // Modify CSR vectors for velocity and wall BCs
   for (i = 0; i<nVelNodes; i++) {
      node = velNodes[i][0];         // Node at which this EBC is specified
   	
      x = coord[node][0];            // May be necessary for BCstring evaluation
      y = coord[node][1];
      z = coord[node][2];

      q=0;
      for(p=rowStartsSmall[node]; p<rowStartsSmall[node+1]; p++) {  // p is the location of the col vector(col[x], p=x) 
         if(colSmall[p] == node) {                                  // Selection process of the KeKMapSmall data from the col vector
            break; 
         }
         q++;
      }

      whichBC = velNodes[i][1]-1;      // Number of the specified BC 
      
      for (j=rowStarts[node]; j<rowStarts[node+1]; j++) {
         val[j] = 0.0;
      }
      val[ rowStarts[node] + q ] = 1 * bigNumber ;
         
      
      for (j=rowStarts[node+NN]; j<rowStarts[node+1+NN]; j++) {
         val[j] = 0.0;
      }
      val[ rowStarts[node + NN] + (rowStartsSmall[node+1]- rowStartsSmall[node]) + q ] = 1 * bigNumber;

      
      for (j=rowStarts[node+2*NN]; j<rowStarts[node+1+2*NN]; j++) {
         val[j] = 0.0;
      }
      val[ rowStarts[node+2*NN] + ((rowStartsSmall[node+1]- rowStartsSmall[node]) * 2) + q ] = 1 * bigNumber;
         
   }   

   // Modify CSR vectors for pressure BCs
   for (i = 0; i<nPressureNodes; i++) {
      node = pressureNodes[i][0];         // Node at which this EBC is specified
   	
      x = coord[node][0];                // May be necessary for BCstring evaluation
      y = coord[node][1];
      z = coord[node][2];

      q=0;
      for(p=rowStartsSmall[node]; p<rowStartsSmall[node+1]; p++) {   // p is the location of the col vector(col[x], p=x) 
         if(colSmall[p] == node) {                                   // Selection process of the KeKMapSmall data from the col vector
            break; 
         }
         q++;
      }
      
      whichBC = pressureNodes[i][1]-1;      // Number of the specified BC   	
      
      for (j=rowStarts[node+3*NN]; j<rowStarts[node+1+3*NN]; j++) {
         val[j] = 0.0;
      }
      val[ rowStarts[node+3*NN] + ((rowStartsSmall[node+1]- rowStartsSmall[node]) * 3) + q ] = 1 * bigNumber;    
   } 
   

   // Deleting the unnecessary arrays for future
   for (i=0; i<nPressureNodes; i++) {
      delete pressureNodes[i];
   }
   delete [] pressureNodes;   
   
   for (i=0; i<nVelNodes; i++) {
      delete velNodes[i];
   }   
   delete [] velNodes;
   delete [] colSmall;
   
   // delete [] rowStartsSmall;

} // End of function ApplyBC()




//------------------------------------------------------------------------------
void solve()
//------------------------------------------------------------------------------
{
   bool err;
   int i,j;


   ////----------------------CONTROL---------------------------------------

   // Creates an output file, named "Control_Output" for K and F
   // outputControl.open(controlFile.c_str(), ios::out);
   // outputControl << NN << endl;
   // outputControl << endl;
   // outputControl.precision(4);
   // for(i=0; i<4*NN; i++) {
      // for(j=0; j<4*NN; j++) {
         // outputControl << fixed << "\t" << K[i][j]  ;   
      // }
      // outputControl << fixed << "\t" << F[i] << endl;
   // }
   // outputControl.close();

   ////----------------------CONTROL---------------------------------------


   u = new real[Ndof];

   #ifdef CUSP
      CUSPsolver();
   #else
      gaussElimination(Ndof, K, F, u, err);
   #endif
   
   // Deleting the unnecessary arrays for future

   delete [] F;   
   
   for (i=0; i<4*NN; i++) {
      delete K[i];
   }   
   delete [] K;

}  // End of function solve()




//------------------------------------------------------------------------------
void postProcess()
//------------------------------------------------------------------------------
{
   // Write the calculated unknowns on the screen.
   // Actually it is a good idea to write the results to an output file named
   // ProblemName.out too.

   printf("\nCalculated unknowns are \n\n");
   printf(" Node      x       y       z         u         v         w       p \n");
   printf("========================================================================\n");
   if (eType == 3) {
      for (int i = 0; i<NN; i++) { 
         printf("%-5d %8.4f %8.4f %8.4f %8.4f %8.4f %8.4f %8.4f\n", i, coord[i][0], coord[i][1], coord[i][2], u[i], u[i+NN], u[i+NN*2], u[i+NN*3]);
      }
   }
   else { 
      for (int i = 0; i<NN; i++) { 
         printf("%-5d %18.8f %18.8f %20.8f\n", i, coord[i][0], coord[i][1], u[i]);
      }
   }   

} // End of function postProcess()




//------------------------------------------------------------------------------
void writeTecplotFile()
//------------------------------------------------------------------------------
{
   // Write the calculated unknowns to a Tecplot file
   double x, y, z;
   int i, e;

   outputFile.open((whichProblem + outputExtension).c_str(), ios::out);
   
   outputFile << "TITLE = " << whichProblem << endl;
   outputFile << "VARIABLES = X,  Y,  Z,  U, V, W, P" << endl;
   
   if (eType == 1) {
      outputFile << "ZONE N=" <<  NN  << " E=" << NE << " F=FEPOINT ET=QUADRILATERAL" << endl;
      }        
   else if (eType == 3) {
      outputFile << "ZONE NODES=" <<  NN  << ", ELEMENTS=" << NE << ", DATAPACKING=POINT, ZONETYPE=FEBRICK" << endl;   
      }
      else { 
      outputFile << "ZONE N=" <<  NN  << " E=" << NE << " F=FEPOINT ET=TRIANGLE" << endl;
      }

   // Print the coordinates and the calculated values
   for (i = 0; i<NN; i++) {
      x = coord[i][0];
      y = coord[i][1];
      z = coord[i][2];  
      outputFile.precision(5);
      outputFile << fixed << "\t" << x << " "  << y << " "  << z << " " << u[i] << " " << u[i+NN] << " " << u[i+NN*2] << " " << u[i+NN*3] << endl;
   }

   // Print the connectivity list
   for (e = 0; e<NE; e++) {
      outputFile << fixed << "\t";
      for (i = 0; i<NENv; i++) {
         // outputFile.precision(5);
         outputFile << LtoG[e][i]+1 << " " ;
      }
      outputFile<< endl;
   }

   outputFile.close();
} // End of function writeTecplotFile()




//-----------------------------------------------------------------------------
void gaussElimination(int N, real **K, real *F, real *u, bool& err)
//-----------------------------------------------------------------------------
{
   // Solve system of N linear equations with N unknowns using Gaussian elimination
   // with scaled partial pivoting.
   // err returns true if process fails; false if it is successful.
   
   int *indx=new int[Ndof];
   real *scale= new real[Ndof];
   real maxRatio, ratio, sum;
   int maxIndx, tmpIndx;;
    
   for (int i = 0; i < N; i++) {
      indx[i] = i;  // Index array initialization
   }
    
   // Determine scale factors
    
   for (int row = 0; row < N; row++) {
      scale[row] = abs(K[row][0]);
      for (int col = 1; col < N; col++) {
         if (abs(K[row][col]) > scale[row]) {
            scale[row] = abs(K[row][col]);
		   }
	   }
   }
    
   // Forward elimination
    
   for (int k = 0; k < N; k++) {
      maxRatio = abs(K[indx[k]][k])/scale[indx[k]];
      maxIndx = k;
      
      for (int i = k+1; i < N; i++) {
         if (abs(K[indx[i]][k])/scale[indx[i]] > maxRatio) {
            maxRatio = abs(K[indx[i]][k])/scale[indx[i]];
            maxIndx = i;
         }
      }

      if (maxRatio == 0) { // no pivot available
         err = true;
         return;
      }

      tmpIndx =indx[k];
      indx[k]=indx[maxIndx]; 
      indx[maxIndx] = tmpIndx;
    
      // Use pivot row to eliminate kth variable in "lower" rows

      for (int i = k+1; i < N; i++) {
         ratio = -K[indx[i]][k]/K[indx[k]][k];
         for (int col = k; col <= N; col++) {
            if (col==N)
               F[indx[i]] += ratio*F[indx[k]];
            else
               K[indx[i]][col] += ratio*K[indx[k]][col];
         }
      }
   }	
    
   // Back substitution

   for (int k = N-1; k >= 0; k--) {
      sum = 0;
      for (int col = k+1; col < N; col++) {
         sum += K[indx[k]][col] * F[indx[col]];
      }
      F[indx[k]] = (F[indx[k]] - sum)/K[indx[k]][k];
   }

   for (int k = 0; k < N; k++) {
      u[k] = F[indx[k]];
   }

} // End of function gaussElimination()

