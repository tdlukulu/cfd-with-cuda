#include <cusp/csr_matrix.h>
#include <cusp/print.h>
#include <cusp/krylov/cg.h>
#include <cusp/krylov/bicg.h>
#include <cusp/krylov/bicgstab.h>
#include <cusp/krylov/gmres.h>

using namespace std;

#ifdef SINGLE
  typedef float real2;
#else
  typedef double real2;
#endif

extern int *rowStartsSmall, *colSmall, NN, NNZ, solverIterMax;
extern double solverTol;
extern real2 *K_1, *F;
extern real2 *u, *v, *w;
extern real2 *u_temp, *v_temp, *w_temp;
extern int phase;

//-----------------------------------------------------------------------------
void CUSP_GMRES()
//-----------------------------------------------------------------------------
{
   // Solve system of linear equations using an iterative solver of CUSP on a GPU.

   //cout << endl << "Start of CUSPsolver() function." << endl;
   //cout << "Ndof = " << Ndof << endl;
   //cout << "NNZ  = " << NNZ << endl;

   // Allocate stifness matrix [A] in CSR format and right hand side vector {b}
   // and solution vector {x} in device memory.
   cusp::csr_matrix<int, real2, cusp::device_memory> A(NN, NN, NNZ);
   cusp::array1d<real2, cusp::device_memory> b(NN);
   cusp::array1d<real2, cusp::device_memory> x(NN);

   // Copy CSR row pointers to device memory
   thrust::copy(rowStartsSmall,rowStartsSmall + NN + 1,A.row_offsets.begin());

   // Copy CSR column indices to device memory
   thrust::copy(colSmall,colSmall +  NNZ,A.column_indices.begin());

   // Copy CSR values to device memory
   thrust::copy(K_1,K_1 + NNZ,A.values.begin()); 

   // Copy right hand side vector to device memory
   thrust::copy(F,F + NN,b.begin());

   // Copy previous solution to device memory
   switch (phase) {
      case 0:
         thrust::copy(u, u + NN, x.begin());      
         break;
      case 1:                 
         thrust::copy(v, v + NN, x.begin());     
         break;
      case 2:                
         thrust::copy(w, w + NN, x.begin());     
         break;
   }
   
   // Set stopping criteria:
   //cusp::verbose_monitor<real2> monitor(b, solverIterMax, solverTol);
   cusp::default_monitor<real2> monitor(b, solverIterMax, solverTol);

   // Set preconditioner (identity)
   cusp::identity_operator<real2, cusp::device_memory> M(A.num_rows, A.num_rows);

   // Solve the linear system A * x = b with the Conjugate Gradient method
   // cusp::krylov::bicgstab(A, x, b, monitor, M);
   int restart = 40;
   // cout << "Iterative solution is started." << endl;
   cusp::krylov::gmres(A, x, b, restart, monitor);
   // cout << "Iterative solution is finished." << endl;

   // Copy x from device back to u on host 
   switch (phase) {
      case 0:
         thrust::copy(x.begin(), x.end(), u_temp);
         break;
      case 1:                 
         thrust::copy(x.begin(), x.end(), v_temp);     
         break;
      case 2:                
         thrust::copy(x.begin(), x.end(), w_temp);     
         break;
   }    

   // report solver results
   if (monitor.converged())
   {
       std::cout << "Solver converged to " << monitor.relative_tolerance() << " relative tolerance";
       std::cout << " after " << monitor.iteration_count() << " iterations";
   }
   else
   {
       std::cout << "Solver reached iteration limit " << monitor.iteration_limit() << " before converging";
       std::cout << " to " << monitor.relative_tolerance() << " relative tolerance ";
   }

}  // End of function CUSP_GMRES()

