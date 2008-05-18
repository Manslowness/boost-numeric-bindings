//
//  Copyright Toon Knapen, Karl Meerbergen
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//

#include "../../blas/test/random.hpp"

#include <boost/numeric/bindings/lapack/hbevx.hpp>
#include <boost/numeric/ublas/matrix_proxy.hpp>
#include <boost/numeric/ublas/vector_proxy.hpp>
#include <boost/numeric/bindings/traits/ublas_matrix.hpp>
#include <boost/numeric/bindings/traits/ublas_vector.hpp>
#include <boost/numeric/bindings/traits/ublas_banded.hpp>
#include <boost/numeric/bindings/traits/ublas_hermitian.hpp>
#include <boost/numeric/ublas/hermitian.hpp>
#include <boost/numeric/ublas/io.hpp>

#include <iostream>
#include <limits>


namespace ublas = boost::numeric::ublas;
namespace lapack = boost::numeric::bindings::lapack;


// Fill a matrix
template <typename M>
void fill(M& m) {
   typedef typename M::size_type  size_type ;
   typedef typename M::value_type value_type ;

   typedef typename boost::numeric::bindings::traits::type_traits<value_type>::real_type real_type ;

   int size = m.size2() ;
   int band = m.upper() ;

   for (int i=0; i<size; ++i) {
      for (int j=std::max(0,i-band); j<i; ++j) m(j,i) = random_value<value_type>();
      m(i,i) = random_value<real_type>();
   }
} // randomize()


template <typename U>
int lower() {
   return 0;
}

template <>
int lower<ublas::lower>() { return 2; }


template <typename U>
int upper() {
   return 0;
}

template <>
int upper<ublas::upper>() { return 2; }

template <typename H, typename E, typename Z>
int check_residual(H const& h, E const& e, Z const& z) {
   typedef typename H::value_type value_type ;
   typedef typename E::value_type real_type ;

   // Check eigen decomposition
   int n = h.size1();
   ublas::matrix<value_type> error( n, n ); error.clear();

   // Copy band matrix in error
   error.assign( h );
   assert( norm_frobenius( error - herm( error ) ) == 0.0 ) ;

   for (int i=0; i<n; ++i) {
      error .minus_assign( outer_prod( column(z, i), e(i) * conj( column(z, i) ) ) ) ;
   }
   return (norm_frobenius( error )
           >= n* norm_2( e ) * std::numeric_limits< real_type >::epsilon() ) ;
} // check_residual()


template <typename T, typename W, typename UPLO, typename Orientation>
int do_memory_uplo(int n, W& workspace ) {
   typedef typename boost::numeric::bindings::traits::type_traits<T>::real_type real_type ;

   typedef ublas::banded_matrix<T, Orientation>      banded_type ;
   typedef ublas::matrix<T, ublas::column_major>     matrix_type ;
   typedef ublas::vector<real_type>                  vector_type ;

   // Set matrix
   banded_type a( n, n, lower<UPLO>(), upper<UPLO>() ); a.clear(); // Make upper triangular band matrix
   matrix_type q( n, n );
   matrix_type z( n, n );
   vector_type e1( n );
   vector_type e2( n );

   fill( a );
   banded_type a2( a );

   ublas::hermitian_adaptor<banded_type, UPLO> h( a ), h2( a2 );
   real_type vl = 0, vu = 1, abstol = 1e-28;
   int il = 1, iu = n-1, m;
   ublas::vector<int> ifail(n);


   // Compute Schur decomposition.
   lapack::hbevx( 'V', 'A', h, q, vl, vu, il, iu, abstol, m, e1, z, ifail, workspace ) ;

   if (check_residual( h2, e1, z )) return 255 ;

   lapack::hbevx( 'N', 'A', h2, q, vl, vu, il, iu, abstol, m, e2, z, ifail, workspace ) ;
   if (norm_2( e1 - e2 ) > n * norm_2( e1 ) * std::numeric_limits< real_type >::epsilon()) return 255 ;

   // Test for a matrix range
   fill( a ); a2.assign( a );

   typedef ublas::matrix_range< banded_type > banded_range ;

   ublas::range r(1,n-1) ;
   banded_range a_r( a, r, r );
   ublas::hermitian_adaptor< banded_range, UPLO> h_r( a_r );
   ublas::vector_range< vector_type> e_r( e1, r );
   ublas::matrix_range< matrix_type> z_r( z, r, r );
   ublas::vector<int> ifail_r(n-2);

   lapack::hbevx( 'V', 'A', h_r, q, vl, vu, il, iu, abstol, m, e_r, z_r, ifail_r, workspace ) ;

   banded_range a2_r( a2, r, r );
   ublas::hermitian_adaptor< banded_range, UPLO> h2_r( a2_r );
   if (check_residual( h2_r, e_r, z_r )) return 255 ;

   return 0 ;
} // do_memory_uplo()


template <typename T, typename W>
int do_memory_type(int n, W workspace) {
   std::cout << "  upper\n" ;
   if (do_memory_uplo<T,W,ublas::upper, ublas::row_major>(n, workspace)) return 255 ;
   std::cout << "  lower\n" ;
   if (do_memory_uplo<T,W,ublas::lower, ublas::row_major>(n, workspace)) return 255 ;
   return 0 ;
}

template <typename T>
struct Workspace {
   typedef ublas::vector< T >                                               array_type ;
   typedef ublas::vector< int >                                             int_array_type ;
   typedef lapack::detail::workspace1< array_type >                         first_type;
   typedef lapack::detail::workspace1< int_array_type >                     second_type;
   typedef std::pair<first_type, second_type>                               type ;

   Workspace(size_t n)
   : work_( 7*n )
   , iwork_( 5*n )
   {}

   type operator() () {
      return type( first_type( work_ ), second_type( iwork_ ) );
   }

   array_type work_ ;
   int_array_type     iwork_ ;
};

template <typename T>
struct Workspace< std::complex<T> > {
   typedef ublas::vector< std::complex<T> >                                 complex_array_type ;
   typedef ublas::vector< T >                                               real_array_type ;
   typedef ublas::vector< int >                                             int_array_type ;
   typedef lapack::detail::workspace2< complex_array_type,real_array_type > first_type;
   typedef lapack::detail::workspace1< int_array_type >                     second_type;
   typedef std::pair<first_type, second_type>                               type ;

   Workspace(size_t n)
   : work_( n )
   , rwork_( 7*n )
   , iwork_( 5*n )
   {}

   type operator() () {
      return type( first_type( work_, rwork_ ), second_type( iwork_ ) );
   }

   complex_array_type work_ ;
   real_array_type    rwork_ ;
   int_array_type     iwork_ ;
};

template <typename T>
int do_value_type() {
   const int n = 8 ;

   std::cout << " optimal workspace\n";
   if (do_memory_type<T,lapack::optimal_workspace>( n, lapack::optimal_workspace() ) ) return 255 ;

   std::cout << " minimal workspace\n";
   if (do_memory_type<T,lapack::minimal_workspace>( n, lapack::minimal_workspace() ) ) return 255 ;

   std::cout << " workspace array\n";
   Workspace<T> work( n );
   do_memory_type<T,typename Workspace<T>::type >( n, work() );
   return 0;
} // do_value_type()


int main() {
   // Run tests for different value_types
   std::cout << "float\n" ;
   if (do_value_type<float>()) return 255;

   std::cout << "double\n" ;
   if (do_value_type<double>()) return 255;

   std::cout << "complex<float>\n" ;
   if (do_value_type< std::complex<float> >()) return 255;

   std::cout << "complex<double>\n" ;
   if (do_value_type< std::complex<double> >()) return 255;

   std::cout << "Regression test succeeded\n" ;
   return 0;
}

