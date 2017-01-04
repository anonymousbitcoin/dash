#ifndef DASH__VIEW__VIEW_MOD_H__INCLUDED
#define DASH__VIEW__VIEW_MOD_H__INCLUDED

#include <dash/Types.h>
#include <dash/Range.h>
#include <dash/Iterator.h>

#include <dash/view/IndexSet.h>
#include <dash/view/ViewTraits.h>

#include <dash/view/Local.h>
#include <dash/view/Global.h>
#include <dash/view/Origin.h>
#include <dash/view/Domain.h>
#include <dash/view/Apply.h>


#ifndef DOXYGEN

/* TODO: Eventually, these probably are not public definitions.
 *       Move to namespace internal.
 *
 * Implementing view modifier chain as combination of command pattern
 * and chain of responsibility pattern.
 * For now, only compile-time projections/slices are supported such as:
 *
 *   sub<0>(10,20).sub<1>(30,40)
 *
 * but not run-time projections/slices like:
 *
 *   sub(0, { 10,20 }).sub(1, { 30,40 })
 *
 * A view composition is a chained application of view modifier types
 * that depend on the type of their predecessor in the chain.
 *
 * Example:
 *
 *  sub<0>(2).sub<1>(3,4)
 *  :         :
 *  |         |
 *  |         '--> ViewSubMod<0, ViewSubMod<-1, ViewOrigin> >
 *  |                            '------------.-----------'
 *  |                                         '--> parent
 *  '--> ViewSubMod<-1, ViewOrigin >
 *                      '----.---'
 *                           '--> parent
 *
 * Consequently, specific ViewMod types are defined for every modifier
 * category.
 * As an alternative, all view modifications could be stored in command
 * objects of a single ViewMod type. Expressions then could not be
 * evalated at compile-time, however.
 *
 * Currently, only two view modifier types seem to be required:
 * - ViewSubMod
 * - ViewBlockMod (-> ViewSubMod)
 * - ViewLocalMod
 *
 * However, View modifier types should subclass a common ViewMod base
 * class - or vice versa, following the policy pattern with the
 * operation specified as policy:
 *
 *   template <dim_t DimDiff, class DomainType>
 *   class ViewMod : DomainType
 *   {
 *      // ...
 *   }
 *
 * or:
 *
 *   template <dim_t DimDiff, class ViewModOperation>
 *   class ViewMod : ViewModOperation
 *   {
 *      // ...
 *   }
 *
 *   class ViewModSubOperation;
 *   // defines
 *   // - sub<N>(...)
 *   // - view_mod_op() { return sub<N>(...); }
 *
 *   ViewMod<0, ViewModSubOperation> view_sub(initializer_list);
 *   // - either calls view_mod_op(initializer_list) in constructor
 *   // - or provides method sub<N>(...) directly
 *
 *
 * TODO: The ViewMod types don't satisfy the View concept entirely as
 *       methods like extents(), offsets(), cannot be defined without
 *       a known pattern type.
 *       Also, view modifiers are not bound to a data domain (like an
 *       array address space), they do not provide access to elements.
 *
 *       Clarify/ensure that these unbound/unmaterialized/lightweight
 *       views cannot appear in expressions where they are considered
 *       as models of the View concept.
 *
 *       Does not seem problematic so far as bound- and unbound views
 *       have different types (would lead to compiler errors in worst
 *       case) and users should not be tempted to access elements 
 *       without specifying a data domain first:
 *
 *          matrix.sub<1>(1,2);            is bound to matrix element
 *                                         domain
 *
 *          os << sub<1>(1,2);             view unbound at this point
 *                                         and value access cannot be
 *                                         specified.
 *          os << sub<3>(10,20) << mat;    bound once container `mat`
 *                                         is passed.
 *
 * TODO: Define dereference operator*() for view types, delegating to
 *       domain::operator* recursively.
 */


namespace dash {

// --------------------------------------------------------------------
// ViewOrigin
// --------------------------------------------------------------------

/**
 * Monotype for the logical symbol that represents a view origin.
 */
class ViewOrigin
{
  typedef ViewOrigin self_t;

public:
  typedef dash::default_index_t   index_type;
  typedef self_t                 domain_type;

public:
  typedef std::integral_constant<bool, false> is_local;

public:
  constexpr const domain_type & domain() const {
    return *this;
  }

  constexpr bool operator==(const self_t & rhs) const {
    return (this == &rhs);
  }
  
  constexpr bool operator!=(const self_t & rhs) const {
    return !(*this == rhs);
  }
};

template <>
struct view_traits<ViewOrigin> {
  typedef ViewOrigin                                           origin_type;
  typedef ViewOrigin                                           domain_type;
  typedef ViewOrigin                                            image_type;
  typedef typename ViewOrigin::index_type                       index_type;

  typedef std::integral_constant<bool, false>                is_projection;
  typedef std::integral_constant<bool, true>                 is_view;
  typedef std::integral_constant<bool, true>                 is_origin;
  typedef std::integral_constant<bool, false>                is_local;
};

// ------------------------------------------------------------------------
// Forward-declaration: ViewSubMod
// ------------------------------------------------------------------------

template <
  dim_t DimDiff,
  class DomainType = ViewOrigin,
  class IndexType  = typename DomainType::IndexType >
class ViewSubMod;


// ------------------------------------------------------------------------
// Forward-declaration: ViewLocalMod
// ------------------------------------------------------------------------

template <
  dim_t DimDiff,
  class DomainType = ViewOrigin,
  class IndexType  = typename DomainType::IndexType >
class ViewLocalMod;

template <
  dim_t DimDiff,
  class DomainType,
  class IndexType >
struct view_traits<ViewLocalMod<DimDiff, DomainType, IndexType> > {
  typedef DomainType                                           domain_type;
  typedef typename dash::view_traits<domain_type>::origin_type origin_type;
  typedef ViewLocalMod<DimDiff, DomainType, IndexType>          image_type;
  typedef IndexType                                             index_type;

  typedef std::integral_constant<bool, (DimDiff != 0)>       is_projection;
  typedef std::integral_constant<bool, true>                 is_view;
  typedef std::integral_constant<bool, false>                is_origin;
  typedef std::integral_constant<bool, true>                 is_local;
};

// ------------------------------------------------------------------------
// ViewLocalMod
// ------------------------------------------------------------------------

template <
  class ViewLocalModType >
class ViewSubLocalIndexSet
{
  typedef ViewSubLocalIndexSet<ViewLocalModType> self_t;
  typedef struct {
    bool begin_ready = false;
    bool end_ready   = false;
  } ready_state;

public:
  constexpr static dim_t dimdiff = 0;

  typedef typename ViewLocalModType::index_type                 index_type;
  typedef typename dash::view_traits<ViewLocalModType>::origin_type
                                                               origin_type;

public:
  ViewSubLocalIndexSet(ViewLocalModType & view_local_mod)
  : _view_local_mod(view_local_mod)
  { }

private:
  constexpr const typename origin_type::pattern_type & pattern() const {
    return dash::origin(_view_local_mod).pattern();
  }

public:
  constexpr index_type begin() const {
    return pattern().local(
             std::max<index_type>(
               dash::begin(_view_local_mod),
               pattern().global(0)
             )
           );
  }

  constexpr index_type end() const {
    return pattern().local(
             std::min<index_type>(
               dash::end(_view_local_mod),
               pattern().global(
                 pattern().local_capacity() - 1
               ) + 1
             )
           );
  }

private:
  ViewLocalModType & _view_local_mod;
  index_type         _begin_index;
  index_type         _end_index;
  ready_state        _ready;
};

// ----------------------------------------------------------------------
// ViewLocalModBase
// ----------------------------------------------------------------------

template <
  class ViewLocalModType,
  bool  LocalOfSub >
class ViewLocalModBase;

// ----------------------------------------------------------------------
// View Local of Sub (array.sub.local)
//                             |
//                             '--> non-trivial case, range calculations
//
template <
  class ViewLocalModType >
class ViewLocalModBase<ViewLocalModType, true>
{
  typedef ViewLocalModBase<ViewLocalModType, true>                 self_t;
  typedef ViewSubLocalIndexSet<self_t>               image_index_set_type;

public:
  constexpr static dim_t dimdiff = 0;

  typedef std::integral_constant<bool, true>  is_local;

  typedef typename view_traits<ViewLocalModType>::domain_type domain_type;
  typedef typename view_traits<ViewLocalModType>::origin_type origin_type;
  typedef typename domain_type::local_type                     local_type;
  typedef image_index_set_type                                 image_type;
  typedef typename view_traits<domain_type>::index_type        index_type;

  ViewLocalModBase(domain_type & domain_sub)
  : _domain(domain_sub), _image_index_set(*this)
  { }

public:
  
  constexpr const image_type & apply() const {
    // e.g. called via
    //   dash::begin(dash::apply(*this))
    return _image_index_set;
  }

protected:
  domain_type                & _domain;
  // range created by application of this view modifier.
  image_index_set_type         _image_index_set;
};


// ----------------------------------------------------------------------
// View Local of Origin (array.local.sub)
//                            |
//                            '--> trivial case
template <
  class ViewLocalModType >
class ViewLocalModBase<ViewLocalModType, false>
{
public:
  constexpr static dim_t dimdiff = 0;

  typedef std::integral_constant<bool, true>  is_local;

  typedef typename view_traits<ViewLocalModType>::domain_type domain_type;
  typedef typename view_traits<ViewLocalModType>::origin_type origin_type;
  typedef typename domain_type::local_type                     local_type;
  typedef typename domain_type::local_type                     image_type;
  typedef typename domain_type::index_type                     index_type;

  ViewLocalModBase(domain_type & domain)
  : _domain(domain)
  { }

public:
  constexpr image_type & apply() const {
    // e.g. called via
    //   dash::begin(dash::apply(*this))
    return dash::local(_domain);
  }

protected:
  domain_type & _domain;
};

// ======================================================================

template <
  dim_t DimDiff,
  class DomainType,
  class IndexType >
class ViewLocalMod
: public dash::ViewLocalModBase<
           ViewLocalMod<DimDiff, DomainType, IndexType>,
           dash::view_traits<DomainType>::is_view::value >
{
private:
  typedef ViewLocalMod<DimDiff, DomainType, IndexType> self_t;

public:
  typedef DomainType                       domain_type;
  typedef IndexType                         index_type;
  typedef self_t                            local_type;
  typedef typename domain_type::local_type  image_type;

public:

  ViewLocalMod() = delete;

  ViewLocalMod(DomainType & domain)
  : ViewLocalModBase<
      ViewLocalMod<DimDiff, DomainType, IndexType>,
      dash::view_traits<DomainType>::is_view::value
    >(domain)
  { }

  constexpr bool operator==(const self_t & rhs) const {
    return (this      == &rhs ||
            // Note: testing _domain for identity (identical address)
            //       instead of equality (identical value)
            &this->_domain == &rhs._domain);
  }
  
  constexpr bool operator!=(const self_t & rhs) const {
    return !(*this == rhs);
  }

  constexpr image_type & begin() const {
    // return dash::local(_domain)
    //      + dash::local(dash::begin(_domain))
    //
    // return dash::local(dash::begin(_domain) +
    //                    dash::global(
    //                      dash::begin(dash::local(_domain))
    //                    ));
    return dash::begin(dash::apply(*this));
  }

  inline image_type & begin() {
    return dash::begin(dash::apply(*this));
  }

  constexpr image_type & end() const {
    return dash::end(dash::apply(*this));
  }

  inline image_type & end() {
    return dash::end(dash::apply(*this));
  }

  constexpr domain_type & domain() const {
    return this->_domain;
  }

  inline domain_type & domain() {
    return this->_domain;
  }

  constexpr local_type & local() const {
    return *this;
  }

  inline local_type & local() {
    return *this;
  }

  constexpr index_type size() const {
    return dash::distance(dash::begin(*this), dash::end(*this));
  }
  
  constexpr bool empty() const {
    return size() == 0;
  }

}; // class ViewLocalMod

// --------------------------------------------------------------------
// ViewSubMod
// --------------------------------------------------------------------

template <
  dim_t DimDiff,
  class DomainType,
  class IndexType >
class ViewSubMod
{
  typedef ViewSubMod<DimDiff, DomainType, IndexType> self_t;

  template <dim_t DD_, class OT_, class IT_>
  friend class ViewLocalMod;

public:
  constexpr static dim_t dimdiff  = DimDiff;

  typedef typename dash::view_traits<DomainType>::is_local is_local;

  typedef DomainType                                      domain_type;
  typedef IndexType                                        index_type;
  typedef ViewLocalMod<DimDiff, self_t, IndexType>         local_type;

public:
  ViewSubMod() = delete;

  ViewSubMod(
    DomainType & domain,
    IndexType    begin,
    IndexType    end)
  : _domain(domain), _begin(begin), _end(end), _local(*this)
  { }

  constexpr bool operator==(const self_t & rhs) const {
    return (this      == &rhs ||
            // Note: testing _domain for identity (identical address)
            //       instead of equality (identical value)
            (&_domain == &rhs._domain &&
             _begin   == rhs._begin &&
             _end     == rhs._end));
  }
  
  constexpr bool operator!=(const self_t & rhs) const {
    return !(*this == rhs);
  }

  constexpr auto begin() const
    -> decltype(dash::begin(dash::domain(*this))) {
    return dash::begin(dash::domain(*this)) + _begin;
  }

  inline auto begin()
    -> decltype(dash::begin(dash::domain(*this))) {
    return dash::begin(dash::domain(*this)) + _begin;
  }

  constexpr auto end() const
    -> decltype(dash::begin(dash::domain(*this))) {
    return dash::begin(dash::domain(*this)) + _end;
  }

  inline auto end()
    -> decltype(dash::begin(dash::domain(*this))) {
    return dash::begin(dash::domain(*this)) + _end;
  }

  constexpr const domain_type & domain() const {
    return _domain;
  }

  inline domain_type & domain() {
    return _domain;
  }

  constexpr const local_type & local() const {
    return _local;
  }

  inline local_type & local() {
    return _local;
  }

  constexpr index_type size() const {
    return dash::distance(dash::begin(*this), dash::end(*this));
  }
  
  constexpr bool empty() const {
    return size() == 0;
  }

private:
  domain_type & _domain;
  index_type    _begin;
  index_type    _end;
  local_type    _local;

}; // class ViewSubMod

} // namespace dash

#endif // DOXYGEN
#endif // DASH__VIEW__VIEW_MOD_H__INCLUDED