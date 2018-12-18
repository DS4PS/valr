#include "valr.h"

// Based on Kevin Ushey's implementation here:
// http://kevinushey.github.io/blog/2015/01/24/understanding-data-frame-subsetting/
DataFrame rowwise_subset_df(const DataFrame& x,
                            IntegerVector row_indices,
                            bool r_index = false) {

  int column_indices_n = x.ncol();
  int row_indices_n = row_indices.size();

  if (r_index) {
    row_indices = row_indices - 1;
  }

  List output = no_init(column_indices_n);

  CharacterVector x_names =
    as<CharacterVector>(Rf_getAttrib(x, R_NamesSymbol));

  output.attr("names") = x_names;

  for (int j = 0; j < column_indices_n; ++j)
  {
    SEXP element = VECTOR_ELT(x, j);

    SEXP vec = PROTECT(
                 Rf_allocVector(TYPEOF(element), row_indices_n)
               );

    for (int i = 0; i < row_indices_n; ++i)
    {
      // if the row_indices contain NA, return type dependent NAs
      // columns can only contain logical, numeric, character, integer, or list types
      switch (TYPEOF(vec))
      {
      case REALSXP:
        if (row_indices[i] == NA_INTEGER) {
          REAL(vec)[i] = Rcpp::Vector<REALSXP>::get_na() ;
        } else {
          REAL(vec)[i] =
            REAL(element)[row_indices[i]];
        }
        break;
      case INTSXP:
      case LGLSXP:
        if (row_indices[i] == NA_INTEGER) {
          INTEGER(vec)[i] = Rcpp::Vector<INTSXP>::get_na() ;
        } else {
          INTEGER(vec)[i] =
            INTEGER(element)[row_indices[i]];
        }
        break;
      case STRSXP:
        if (row_indices[i] == NA_INTEGER) {
          SET_STRING_ELT(vec, i,
                         Rcpp::Vector<STRSXP>::get_na()) ;
          break;
        }
        SET_STRING_ELT(vec, i,
                       STRING_ELT(element, row_indices[i]));
        break;
      case VECSXP:
        if (row_indices[i] == NA_INTEGER) {
          SET_VECTOR_ELT(vec, i,
                         Rcpp::Vector<VECSXP>::get_na()) ;
        } else {
          SET_VECTOR_ELT(vec, i,
                         VECTOR_ELT(element, row_indices[i]));
        }
        break;
      default: {
        stop("Incompatible column type detected");
      }
      }
    }

    UNPROTECT(1);

    SET_VECTOR_ELT(output, j, vec);
  }

  Rf_copyMostAttrib(x, output);

  Rf_setAttrib(output, R_RowNamesSymbol,
               IntegerVector::create(NA_INTEGER, -row_indices_n));

  return output;

}

DataFrame rowwise_subset_df(const DataFrame& x,
                            std::vector<int> row_indices,
                            bool r_index = false) {

  int column_indices_n = x.ncol();
  int row_indices_n = row_indices.size();

  if (r_index) {
    for (auto i:row_indices) {
      i--;
    }
  }

  List output = no_init(column_indices_n);

  CharacterVector x_names =
    as<CharacterVector>(Rf_getAttrib(x, R_NamesSymbol));

  output.attr("names") = x_names;

  for (int j = 0; j < column_indices_n; ++j)
  {
    SEXP element = VECTOR_ELT(x, j);

    SEXP vec = PROTECT(
                 Rf_allocVector(TYPEOF(element), row_indices_n)
               );

    for (int i = 0; i < row_indices_n; ++i)
    {
      // if the row_indices contain NA, return type dependent NAs
      // columns can only contain logical, numeric, character, integer, or list types
      switch (TYPEOF(vec))
      {
      case REALSXP:
        if (row_indices[i] == NA_INTEGER) {
          REAL(vec)[i] = Rcpp::Vector<REALSXP>::get_na() ;
        } else {
          REAL(vec)[i] =
            REAL(element)[row_indices[i]];
        }
        break;
      case INTSXP:
      case LGLSXP:
        if (row_indices[i] == NA_INTEGER) {
          INTEGER(vec)[i] = Rcpp::Vector<INTSXP>::get_na() ;
        } else {
          INTEGER(vec)[i] =
            INTEGER(element)[row_indices[i]];
        }
        break;
      case STRSXP:
        if (row_indices[i] == NA_INTEGER) {
          SET_STRING_ELT(vec, i,
                         Rcpp::Vector<STRSXP>::get_na()) ;
          break;
        }
        SET_STRING_ELT(vec, i,
                       STRING_ELT(element, row_indices[i]));
        break;
      case VECSXP:
        if (row_indices[i] == NA_INTEGER) {
          SET_VECTOR_ELT(vec, i,
                         Rcpp::Vector<VECSXP>::get_na()) ;
        } else {
          SET_VECTOR_ELT(vec, i,
                         VECTOR_ELT(element, row_indices[i]));
        }
        break;
      default: {
        stop("Incompatible column type detected");
      }
      }
    }

    UNPROTECT(1);

    SET_VECTOR_ELT(output, j, vec);
  }

  Rf_copyMostAttrib(x, output);

  Rf_setAttrib(output, R_RowNamesSymbol,
               IntegerVector::create(NA_INTEGER, -row_indices_n));

  return output;

}

DataFrame subset_dataframe(const DataFrame& df,
                           std::vector<int> indices) {

  DataFrame out = rowwise_subset_df(df, indices, false);
  return (out) ;
}

DataFrame subset_dataframe(const DataFrame& df,
                           IntegerVector indices) {

  DataFrame out = rowwise_subset_df(df, indices, false);
  return (out) ;
}

ValrGroupedDataFrame::ValrGroupedDataFrame(DataFrame x):
  data_(check_is_grouped(x)),
  groups(data_.attr("groups"))
{}

// return data.frame without .rows column (always last in group_data)
DataFrame extract_groups(const DataFrame& x) {
  int n = x.ncol() - 1;
  CharacterVector df_names = x.names() ;

  List res(n);
  CharacterVector new_names(n) ;

  for (int i = 0; i < n; i++) {
    res[i] = x[i] ;
    new_names[i] = df_names[i] ;
  }

  set_rownames(res, x.nrow()) ;
  res.names() = new_names ;
  res.attr("class") = "data.frame" ;
  return res;
}

// Perform intersect between group_data of groupeddataframes and return the
// row indicies shared between both dataframes. Used to identify matching
// groups for two table operations i.e. intersect_impl.
// Based on intersect_data_frame from dplyr
// std::vector<int> shared_row_indexes(const ValrGroupedDataFrame& x,
//                                     const ValrGroupedDataFrame& y) {
//
//   DataFrame grp_x = extract_groups(x.group_data()) ;
//   DataFrame grp_y = extract_groups(y.group_data()) ;
//
//   typedef VisitorSetIndexSet<DataFrameJoinVisitors> Set;
//
//   SymbolVector x_names = grp_x.names();
//   DataFrameJoinVisitors visitors(grp_x, grp_y, x_names, x_names, true, true);
//   Set set(visitors);
//
//   train_insert(set, grp_x.nrows());
//
//   std::vector<int> indices ;
//   int n_y = grp_y.nrows();
//   for (int i = 0; i < n_y; i++) {
//     Set::iterator it = set.find(-i - 1);
//     if (it != set.end()) {
//       indices.push_back(*it);
//       set.erase(it);
//     }
//   }
//   return indices ;
// }


