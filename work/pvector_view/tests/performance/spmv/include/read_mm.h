#ifndef READ_MM_H
#define READ_MM_H

#include <iostream>
#include <vector>
#include <mmio.h>

void coo_to_csr(int n_row, int n_col, int nnz,
                std::vector<int> const & Ai, std::vector<int> const & Aj, std::vector<double> const & Ax,
                std::vector<int> & rows, std::vector<int> & indices, std::vector<double> & values
               )
{
    // compute number of non-zero entries per row of A
    rows = std::vector<int>(n_row + 1);
    indices = std::vector<int>(nnz);
    values  = std::vector<double>(nnz);

    // increment the corresponding row for each nnz value located in row i
    for (int n = 0; n < nnz; n++){
        rows[ Ai[n] ]++;
    }

    //cumsum to get the offsets in rows
    for(int i = 0, cumsum = 0; i < n_row; i++){
        int temp = rows[i];
        rows[i] = cumsum;
        cumsum += temp;
    }
    rows[n_row] = nnz;

    for(int n = 0; n < nnz; n++){
        int i  = Ai[n];
        int offset = rows[i];

        indices[offset] = Aj[n];
        values[offset]  = Ax[n];
        rows[i]++; /* Prepare the next offset for value located in row i */
    }

    // Retrieve the right values in rows
    for(int i = 0, last = 0; i <= n_row; i++){
        int temp = rows[i];
        rows[i]  = last;
        last   = temp;
    }
}


void read_mm(std::string const & filename, int & M, int & N, int & nnz, std::vector<int> & rows, std::vector<int> & indices, std::vector<double> & values)
{
    int ret_code;
    MM_typecode matcode;
    FILE *f;

    try
    {
        if ((f = fopen(filename.c_str(), "r")) == NULL)  throw 0;

        if (mm_read_banner(f, &matcode) != 0)
        {
            std::cout << "Could not process Matrix Market banner." << std::endl;
            throw 1;
        }

        if ( !mm_is_real(matcode) || !mm_is_sparse(matcode) )
        {
            std::cout << "Could process only real sparse matrices." << std::endl;
            throw 2;
        }

        /* find out size of sparse matrix .... */

        if ((ret_code = mm_read_mtx_crd_size(f, &M, &N, &nnz)) !=0)
            throw 3;
    }

    catch(int param)
    {
        std::cout << "read_mm: caught exception: Exception No. " << param << '\n';
    }

    std::vector<int> I(nnz);
    std::vector<int> J(nnz);
    std::vector<double> val(nnz);

    for (int i=0; i<nnz; i++)
    {
        fscanf(f, "%d %d %lg\n", &I[i], &J[i], &val[i]);
        I[i]--;  /* adjust from 1-based to 0-based */
        J[i]--;
    }

    if (f !=stdin) fclose(f);

    coo_to_csr(M,N,nnz,I,J,val,rows,indices,values);
}


#endif
