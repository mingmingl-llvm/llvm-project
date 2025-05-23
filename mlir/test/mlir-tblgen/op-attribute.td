// RUN: mlir-tblgen -gen-op-decls -I %S/../../include %s | FileCheck %s --check-prefix=DECL
// RUN: mlir-tblgen -gen-op-defs -I %S/../../include %s | FileCheck %s --check-prefix=DEF
// RUN: mlir-tblgen -print-records -I %S/../../include %s | FileCheck %s --check-prefix=RECORD

include "mlir/IR/AttrTypeBase.td"
include "mlir/IR/EnumAttr.td"
include "mlir/IR/OpBase.td"

def Test_Dialect : Dialect {
  let name = "test";
  let cppNamespace = "foobar";
}
class NS_Op<string mnemonic, list<Trait> traits> :
    Op<Test_Dialect, mnemonic, traits>;

def SomeAttr : Attr<CPred<"some-condition">, "some attribute kind"> {
  let storageType = "some-attr-kind";
  let returnType = "some-return-type";
  let convertFromStorage = "$_self.some-convert-from-storage()";
  let constBuilderCall = "some-const-builder-call($_builder, $0)";
}

def SomeAttrDef : AttrDef<Test_Dialect, "SomeAttr"> {
  let attrName = "test.some_attr";
}


// Test required, optional, default-valued attributes
// ---

def AOp : NS_Op<"a_op", []> {
  let arguments = (ins
      SomeAttr:$aAttr,
      DefaultValuedAttr<SomeAttr, "4.2">:$bAttr,
      OptionalAttr<SomeAttr>:$cAttr,
      DefaultValuedOptionalAttr<SomeAttr, "4.2">:$dAttr
  );
}

// DECL-LABEL: AOp declarations

// Test attribute name methods
// ---

// DECL:      static ::llvm::ArrayRef<::llvm::StringRef> getAttributeNames()
// DECL-NEXT:   static ::llvm::StringRef attrNames[] =
// DECL-SAME:     {::llvm::StringRef("aAttr"), ::llvm::StringRef("bAttr"), ::llvm::StringRef("cAttr"), ::llvm::StringRef("dAttr")};
// DECL-NEXT:   return ::llvm::ArrayRef(attrNames);

// DECL:      ::mlir::StringAttr getAAttrAttrName()
// DECL-NEXT:      return getAttributeNameForIndex(0);
// DECL:      ::mlir::StringAttr getAAttrAttrName(::mlir::OperationName name)
// DECL-NEXT:      return getAttributeNameForIndex(name, 0);

// DECL:      ::mlir::StringAttr getBAttrAttrName()
// DECL-NEXT:      return getAttributeNameForIndex(1);
// DECL:      ::mlir::StringAttr getBAttrAttrName(::mlir::OperationName name)
// DECL-NEXT:      return getAttributeNameForIndex(name, 1);

// DECL:      ::mlir::StringAttr getCAttrAttrName()
// DECL-NEXT:      return getAttributeNameForIndex(2);
// DECL:      ::mlir::StringAttr getCAttrAttrName(::mlir::OperationName name)
// DECL-NEXT:      return getAttributeNameForIndex(name, 2);

// DEF-LABEL: AOp definitions

// Test verify method
// ---

// DEF:      ::llvm::LogicalResult AOpAdaptor::verify
// DEF-NEXT: auto tblgen_aAttr = getProperties().aAttr; (void)tblgen_aAttr;
// DEF-NEXT: if (!tblgen_aAttr) return emitError(loc, "'test.a_op' op ""requires attribute 'aAttr'");
// DEF-NEXT: auto tblgen_bAttr = getProperties().bAttr; (void)tblgen_bAttr;
// DEF-NEXT: auto tblgen_cAttr = getProperties().cAttr; (void)tblgen_cAttr;
// DEF-NEXT: auto tblgen_dAttr = getProperties().dAttr; (void)tblgen_dAttr;

// DEF:      if (tblgen_aAttr && !((some-condition)))
// DEF-NEXT:   return emitError(loc, "'test.a_op' op ""attribute 'aAttr' failed to satisfy constraint: some attribute kind");
// DEF:      if (tblgen_bAttr && !((some-condition)))
// DEF-NEXT:   return emitError(loc, "'test.a_op' op ""attribute 'bAttr' failed to satisfy constraint: some attribute kind");
// DEF:      if (tblgen_cAttr && !((some-condition)))
// DEF-NEXT:   return emitError(loc, "'test.a_op' op ""attribute 'cAttr' failed to satisfy constraint: some attribute kind");
// DEF:      if (tblgen_dAttr && !((some-condition)))
// DEF-NEXT:   return emitError(loc, "'test.a_op' op ""attribute 'dAttr' failed to satisfy constraint: some attribute kind");

// Test getter methods
// ---

// DECL:      some-attr-kind getAAttrAttr()
// DECL-NEXT:   ::llvm::cast<some-attr-kind>(getProperties().aAttr)
// DEF:      some-return-type AOp::getAAttr() {
// DEF-NEXT:   auto attr = getAAttrAttr()
// DEF-NEXT:   return attr.some-convert-from-storage();

// DECL:      some-attr-kind getBAttrAttr()
// DECL-NEXT:   ::llvm::dyn_cast_or_null<some-attr-kind>(getProperties().bAttr)
// DEF:      some-return-type AOp::getBAttr() {
// DEF-NEXT:   auto attr = getBAttrAttr();
// DEF-NEXT:   return attr.some-convert-from-storage();

// DECL:      some-attr-kind getCAttrAttr()
// DECL-NEXT:   ::llvm::dyn_cast_or_null<some-attr-kind>(getProperties().cAttr)
// DEF:      ::std::optional<some-return-type> AOp::getCAttr() {
// DEF-NEXT:   auto attr = getCAttrAttr()
// DEF-NEXT:   return attr ? ::std::optional<some-return-type>(attr.some-convert-from-storage()) : (::std::nullopt);

// DECL:      some-attr-kind getDAttrAttr()
// DECL-NEXT:   ::llvm::dyn_cast_or_null<some-attr-kind>(getProperties().dAttr)
// DEF:      some-return-type AOp::getDAttr() {
// DEF-NEXT:   auto attr = getDAttrAttr();
// DEF-NEXT:   if (!attr)
// DEF-NEXT:       return some-const-builder-call(::mlir::Builder((*this)->getContext()), 4.2).some-convert-from-storage();
// DEF-NEXT:   return attr.some-convert-from-storage();

// Test setter methods
// ---

// DECL:      void setAAttrAttr(some-attr-kind attr) {
// DECL-NEXT:   getProperties().aAttr = attr;
// DEF:      void AOp::setAAttr(some-return-type attrValue) {
// DEF-NEXT:   getProperties().aAttr = some-const-builder-call(::mlir::Builder((*this)->getContext()), attrValue);
// DECL:      void setBAttrAttr(some-attr-kind attr) {
// DECL-NEXT:   getProperties().bAttr = attr;
// DEF:      void AOp::setBAttr(some-return-type attrValue) {
// DEF-NEXT:   getProperties().bAttr = some-const-builder-call(::mlir::Builder((*this)->getContext()), attrValue);
// DECL:      void setCAttrAttr(some-attr-kind attr) {
// DECL-NEXT:   getProperties().cAttr = attr;
// DEF:      void AOp::setCAttr(::std::optional<some-return-type> attrValue) {
// DEF-NEXT:   auto &odsProp = getProperties().cAttr;
// DEF-NEXT:   if (attrValue)
// DEF-NEXT:     odsProp = some-const-builder-call(::mlir::Builder((*this)->getContext()), *attrValue);
// DEF-NEXT:   else
// DEF-NEXT:     odsProp = nullptr;

// Test remove methods
// ---

// DECL: ::mlir::Attribute removeCAttrAttr() {
// DECL-NEXT: auto attr = getProperties().cAttr;
// DECL-NEXT: getProperties().cAttr = {};
// DECL-NEXT: return attr;

// Test build methods
// ---

// DEF:      void AOp::build(
// DEF:        odsState.getOrAddProperties<Properties>().aAttr = aAttr;
// DEF:        odsState.getOrAddProperties<Properties>().bAttr = bAttr;
// DEF:        if (cAttr) {
// DEF-NEXT:     odsState.getOrAddProperties<Properties>().cAttr = cAttr;

// DEF:        odsState.getOrAddProperties<Properties>().aAttr = some-const-builder-call(odsBuilder, aAttr);
// DEF-NEXT:   odsState.getOrAddProperties<Properties>().bAttr = some-const-builder-call(odsBuilder, bAttr);
// DEF-NEXT:   if (cAttr) {
// DEF-NEXT:   odsState.getOrAddProperties<Properties>().cAttr = cAttr;
// DEF-NEXT:   }
// DEF-NOT:    if (dAttr)
// DEF:        odsState.getOrAddProperties<Properties>().dAttr = some-const-builder-call(odsBuilder, dAttr);

// DEF:      void AOp::build(
// DEF:        some-return-type aAttr, some-return-type bAttr, /*optional*/some-attr-kind cAttr
// DEF:        odsState.getOrAddProperties<Properties>().aAttr = some-const-builder-call(odsBuilder, aAttr);

// DEF:      void AOp::build(
// DEF:        ::llvm::ArrayRef<::mlir::NamedAttribute> attributes
// DEF:      odsState.addAttributes(attributes);

// DEF:      void AOp::build(
// DEF-SAME:   const Properties &properties,
// DEF-SAME:   ::llvm::ArrayRef<::mlir::NamedAttribute> discardableAttributes
// DEF:      odsState.useProperties(const_cast<Properties&>(properties));
// DEF:      odsState.addAttributes(discardableAttributes);

// DEF:      void AOp::populateDefaultProperties

// Test the above but with prefix.

def Test2_Dialect : Dialect {
  let name = "test2";
  let cppNamespace = "foobar2";
}
def AgetOp : Op<Test2_Dialect, "a_get_op", []> {
  let arguments = (ins
      SomeAttr:$aAttr,
      DefaultValuedOptionalAttr<SomeAttr, "4.2">:$bAttr,
      OptionalAttr<SomeAttr>:$cAttr
  );
}

// DECL-LABEL: AgetOp declarations

// Test attribute name methods
// ---

// DECL:      static ::llvm::ArrayRef<::llvm::StringRef> getAttributeNames()
// DECL-NEXT:   static ::llvm::StringRef attrNames[] =
// DECL-SAME:     {::llvm::StringRef("aAttr"), ::llvm::StringRef("bAttr"), ::llvm::StringRef("cAttr")};
// DECL-NEXT:   return ::llvm::ArrayRef(attrNames);

// DECL:      ::mlir::StringAttr getAAttrAttrName()
// DECL-NEXT:      return getAttributeNameForIndex(0);
// DECL:      ::mlir::StringAttr getAAttrAttrName(::mlir::OperationName name)
// DECL-NEXT:      return getAttributeNameForIndex(name, 0);

// DECL:      ::mlir::StringAttr getBAttrAttrName()
// DECL-NEXT:      return getAttributeNameForIndex(1);
// DECL:      ::mlir::StringAttr getBAttrAttrName(::mlir::OperationName name)
// DECL-NEXT:      return getAttributeNameForIndex(name, 1);

// DECL:      ::mlir::StringAttr getCAttrAttrName()
// DECL-NEXT:      return getAttributeNameForIndex(2);
// DECL:      ::mlir::StringAttr getCAttrAttrName(::mlir::OperationName name)
// DECL-NEXT:      return getAttributeNameForIndex(name, 2);

// DEF-LABEL: AgetOp definitions

// Test verify method
// ---

// DEF:      ::llvm::LogicalResult AgetOpAdaptor::verify
// DEF: auto tblgen_aAttr = getProperties().aAttr; (void)tblgen_aAttr;
// DEF: if (!tblgen_aAttr) return emitError(loc, "'test2.a_get_op' op ""requires attribute 'aAttr'");
// DEF: auto tblgen_bAttr = getProperties().bAttr; (void)tblgen_bAttr;
// DEF: auto tblgen_cAttr = getProperties().cAttr; (void)tblgen_cAttr;
// DEF:      if (tblgen_bAttr && !((some-condition)))
// DEF-NEXT:   return emitError(loc, "'test2.a_get_op' op ""attribute 'bAttr' failed to satisfy constraint: some attribute kind");
// DEF:      if (tblgen_cAttr && !((some-condition)))
// DEF-NEXT:   return emitError(loc, "'test2.a_get_op' op ""attribute 'cAttr' failed to satisfy constraint: some attribute kind");

// Test getter methods
// ---

// DECL:      some-attr-kind getAAttrAttr()
// DECL-NEXT:   ::llvm::cast<some-attr-kind>(getProperties().aAttr)
// DEF:      some-return-type AgetOp::getAAttr() {
// DEF-NEXT:   auto attr = getAAttrAttr()
// DEF-NEXT:   return attr.some-convert-from-storage();

// DECL:      some-attr-kind getBAttrAttr()
// DECL-NEXT:   return ::llvm::dyn_cast_or_null<some-attr-kind>(getProperties().bAttr)
// DEF:      some-return-type AgetOp::getBAttr() {
// DEF-NEXT:   auto attr = getBAttrAttr();
// DEF-NEXT:   if (!attr)
// DEF-NEXT:       return some-const-builder-call(::mlir::Builder((*this)->getContext()), 4.2).some-convert-from-storage();
// DEF-NEXT:   return attr.some-convert-from-storage();

// DECL:      some-attr-kind getCAttrAttr()
// DECL-NEXT:   return ::llvm::dyn_cast_or_null<some-attr-kind>(getProperties().cAttr)
// DEF:      ::std::optional<some-return-type> AgetOp::getCAttr() {
// DEF-NEXT:   auto attr = getCAttrAttr()
// DEF-NEXT:   return attr ? ::std::optional<some-return-type>(attr.some-convert-from-storage()) : (::std::nullopt);

// Test setter methods
// ---

// DECL:      void setAAttrAttr(some-attr-kind attr) {
// DECL-NEXT:   getProperties().aAttr = attr;
// DECL:      void setBAttrAttr(some-attr-kind attr) {
// DECL-NEXT:   getProperties().bAttr = attr;
// DECL:      void setCAttrAttr(some-attr-kind attr) {
// DECL-NEXT:   getProperties().cAttr = attr;

// Test remove methods
// ---

// DECL: ::mlir::Attribute removeCAttrAttr() {
// DECL-NEXT: auto attr = getProperties().cAttr;
// DECL-NEXT: getProperties().cAttr = {};
// DECL-NEXT: return attr;

// Test build methods
// ---

// DEF:      void AgetOp::build(
// DEF:        odsState.getOrAddProperties<Properties>().aAttr = aAttr;
// DEF:        odsState.getOrAddProperties<Properties>().bAttr = bAttr;
// DEF:        if (cAttr) {
// DEF-NEXT:     odsState.getOrAddProperties<Properties>().cAttr = cAttr;

// DEF:      void AgetOp::build(
// DEF:        some-return-type aAttr, /*optional*/some-return-type bAttr, /*optional*/some-attr-kind cAttr
// DEF:        odsState.getOrAddProperties<Properties>().aAttr = some-const-builder-call(odsBuilder, aAttr);

// DEF:      void AgetOp::build(
// DEF:        ::llvm::ArrayRef<::mlir::NamedAttribute> attributes
// DEF:      odsState.addAttributes(attributes);

// DEF:      void AgetOp::build(
// DEF-SAME:   const Properties &properties
// DEF-SAME:   ::llvm::ArrayRef<::mlir::NamedAttribute> discardableAttributes
// DEF:      odsState.useProperties(const_cast<Properties&>(properties));
// DEF:      odsState.addAttributes(discardableAttributes);

// Test the above but using properties.
def ApropOp : NS_Op<"a_prop_op", []> {
  let arguments = (ins
      Property<"unsigned">:$aAttr,
      DefaultValuedAttr<SomeAttr, "4.2">:$bAttr
  );
}

// DEF-LABEL: ApropOp definitions
// DEF:       void ApropOp::populateDefaultProperties

def SomeTypeAttr : TypeAttrBase<"SomeType", "some type attribute">;

def BOp : NS_Op<"b_op", []> {
  let arguments = (ins
    AnyAttr:$any_attr,
    BoolAttr:$bool_attr,
    I32Attr:$i32_attr,
    I64Attr:$i64_attr,
    F32Attr:$f32_attr,
    F64Attr:$f64_attr,
    StrAttr:$str_attr,
    ElementsAttr:$elements_attr,
    FlatSymbolRefAttr:$function_attr,
    SomeTypeAttr:$some_type_attr,
    ArrayAttr:$array_attr,
    TypedArrayAttrBase<SomeAttr, "SomeAttr array">:$some_attr_array,
    TypeAttr:$type_attr
  );
}


// Test common attribute kinds' constraints
// ---

// DEF-LABEL: BOpAdaptor::verify
// DEF: if (tblgen_any_attr && !((true)))
// DEF: if (tblgen_bool_attr && !((::llvm::isa<::mlir::BoolAttr>(tblgen_bool_attr))))
// DEF: if (tblgen_i32_attr && !(((::llvm::isa<::mlir::IntegerAttr>(tblgen_i32_attr))) && ((::llvm::cast<::mlir::IntegerAttr>(tblgen_i32_attr).getType().isSignlessInteger(32)))))
// DEF: if (tblgen_i64_attr && !(((::llvm::isa<::mlir::IntegerAttr>(tblgen_i64_attr))) && ((::llvm::cast<::mlir::IntegerAttr>(tblgen_i64_attr).getType().isSignlessInteger(64)))))
// DEF: if (tblgen_f32_attr && !(((::llvm::isa<::mlir::FloatAttr>(tblgen_f32_attr))) && ((::llvm::cast<::mlir::FloatAttr>(tblgen_f32_attr).getType().isF32()))))
// DEF: if (tblgen_f64_attr && !(((::llvm::isa<::mlir::FloatAttr>(tblgen_f64_attr))) && ((::llvm::cast<::mlir::FloatAttr>(tblgen_f64_attr).getType().isF64()))))
// DEF: if (tblgen_str_attr && !((::llvm::isa<::mlir::StringAttr>(tblgen_str_attr))))
// DEF: if (tblgen_elements_attr && !((::llvm::isa<::mlir::ElementsAttr>(tblgen_elements_attr))))
// DEF: if (tblgen_function_attr && !((::llvm::isa<::mlir::FlatSymbolRefAttr>(tblgen_function_attr))))
// DEF: if (tblgen_some_type_attr && !(((::llvm::isa<::mlir::TypeAttr>(tblgen_some_type_attr))) && ((::llvm::isa<SomeType>(::llvm::cast<::mlir::TypeAttr>(tblgen_some_type_attr).getValue()))) && ((true))))
// DEF: if (tblgen_array_attr && !((::llvm::isa<::mlir::ArrayAttr>(tblgen_array_attr))))
// DEF: if (tblgen_some_attr_array && !(((::llvm::isa<::mlir::ArrayAttr>(tblgen_some_attr_array))) && (::llvm::all_of(::llvm::cast<::mlir::ArrayAttr>(tblgen_some_attr_array), [&](::mlir::Attribute attr) { return attr && ((some-condition)); }))))
// DEF: if (tblgen_type_attr && !(((::llvm::isa<::mlir::TypeAttr>(tblgen_type_attr))) && ((::llvm::isa<::mlir::Type>(::llvm::cast<::mlir::TypeAttr>(tblgen_type_attr).getValue()))) && ((true))))

// Test common attribute kind getters' return types
// ---

// DEF: ::mlir::Attribute BOp::getAnyAttr()
// DEF: bool BOp::getBoolAttr()
// DEF: uint32_t BOp::getI32Attr()
// DEF: uint64_t BOp::getI64Attr()
// DEF: ::llvm::APFloat BOp::getF32Attr()
// DEF: ::llvm::APFloat BOp::getF64Attr()
// DEF: ::llvm::StringRef BOp::getStrAttr()
// DEF: ::mlir::ElementsAttr BOp::getElementsAttr()
// DEF: ::llvm::StringRef BOp::getFunctionAttr()
// DEF: SomeType BOp::getSomeTypeAttr()
// DEF: ::mlir::ArrayAttr BOp::getArrayAttr()
// DEF: ::mlir::ArrayAttr BOp::getSomeAttrArray()
// DEF: ::mlir::Type BOp::getTypeAttr()

// Test building constant values for array attribute kinds
// ---

def COp : NS_Op<"c_op", []> {
  let arguments = (ins
    DefaultValuedOptionalAttr<I32ArrayAttr, "{1, 2}">:$i32_array_attr,
    DefaultValuedOptionalAttr<I64ArrayAttr, "{3, 4}">:$i64_array_attr,
    DefaultValuedOptionalAttr<F32ArrayAttr, "{5.f, 6.f}">:$f32_array_attr,
    DefaultValuedOptionalAttr<F64ArrayAttr, "{7., 8.}">:$f64_array_attr,
    DefaultValuedOptionalAttr<StrArrayAttr, "{\"a\", \"b\"}">:$str_array_attr
  );
}

// DEF-LABEL: COp definitions
// DEF: ::mlir::Builder((*this)->getContext()).getI32ArrayAttr({1, 2})
// DEF: ::mlir::Builder((*this)->getContext()).getI64ArrayAttr({3, 4})
// DEF: ::mlir::Builder((*this)->getContext()).getF32ArrayAttr({5.f, 6.f})
// DEF: ::mlir::Builder((*this)->getContext()).getF64ArrayAttr({7., 8.})
// DEF: ::mlir::Builder((*this)->getContext()).getStrArrayAttr({"a", "b"})


// Test builder method which takes unwrapped values for attributes
// ---

def I32Case5:  I32EnumAttrCase<"case5", 5>;
def I32Case10: I32EnumAttrCase<"case10", 10>;

def SomeI32Enum: I32EnumAttr<
  "SomeI32Enum", "", [I32Case5, I32Case10]>;

def DOp : NS_Op<"d_op", []> {
  let arguments = (ins
    I32Attr:$i32_attr,
    F64Attr:$f64_attr,
    StrAttr:$str_attr,
    BoolAttr:$bool_attr,
    SomeI32Enum:$enum_attr,
    DefaultValuedAttr<I32Attr, "42">:$dv_i32_attr,
    DefaultValuedAttr<F64Attr, "8.">:$dv_f64_attr,
    DefaultValuedStrAttr<StrAttr, "abc">:$dv_str_attr,
    DefaultValuedAttr<BoolAttr, "true">:$dv_bool_attr,
    DefaultValuedAttr<SomeI32Enum, "::SomeI32Enum::case5">:$dv_enum_attr
  );
}

// DECL-LABEL: DOp declarations
// DECL: static void build({{.*}}, uint32_t i32_attr, ::llvm::APFloat f64_attr, ::llvm::StringRef str_attr, bool bool_attr, ::SomeI32Enum enum_attr, uint32_t dv_i32_attr, ::llvm::APFloat dv_f64_attr, ::llvm::StringRef dv_str_attr = "abc", bool dv_bool_attr = true, ::SomeI32Enum dv_enum_attr = ::SomeI32Enum::case5)

// DEF-LABEL: DOp definitions
// DEF: odsState.getOrAddProperties<Properties>().str_attr = odsBuilder.getStringAttr(str_attr);
// DEF: odsState.getOrAddProperties<Properties>().dv_str_attr = odsBuilder.getStringAttr(dv_str_attr);


// Test default dictionary attribute.
// ---

def DefaultDictAttrOp : NS_Op<"default_dict_attr_op", []> {
  let arguments = (ins
    DefaultValuedAttr<DictionaryAttr, "{}">:$empty,
    DefaultValuedAttr<DictionaryAttr, "getDefaultDictAttrs($_builder)">:$non_empty
  );
}

// DEF-LABEL: DefaultDictAttrOp definitions
// DEF: if (!properties.empty)
// DEF:   properties.empty = odsBuilder.getDictionaryAttr({});
// DEF: if (!properties.non_empty)
// DEF:   properties.non_empty = odsBuilder.getDictionaryAttr(getDefaultDictAttrs(odsBuilder));

// DECL-LABEL: DefaultDictAttrOp declarations
// DECL: build(::mlir::OpBuilder &odsBuilder, ::mlir::OperationState &odsState, ::mlir::DictionaryAttr empty = nullptr, ::mlir::DictionaryAttr non_empty = nullptr)


// Test derived type attr.
// ---
def DerivedTypeAttrOp : NS_Op<"derived_type_attr_op", []> {
  let results = (outs AnyTensor:$output);
  DerivedTypeAttr element_dtype = DerivedTypeAttr<"return output().getType();">;
}

// DECL: class DerivedTypeAttrOp : public ::mlir::Op
// DECL-SAME: DerivedAttributeOpInterface::Trait
// DECL: static bool isDerivedAttribute
// DEF: bool DerivedTypeAttrOp::isDerivedAttribute(::llvm::StringRef name) {
// DEF:   if (name == "element_dtype") return true;
// DEF:   return false;
// DEF: }
// DEF: DerivedTypeAttrOp::materializeDerivedAttributes

// Test that only default valued attributes at the end of the arguments
// list get default values in the builder signature
// ---

def EOp : NS_Op<"e_op", []> {
  let arguments = (ins
    I32Attr:$i32_attr,
    DefaultValuedAttr<I32Attr, "42">:$dv_i32_attr,
    F64Attr:$f64_attr,
    DefaultValuedAttr<F64Attr, "8.">:$dv_f64_attr,
    StrAttr:$str_attr,
    DefaultValuedStrAttr<StrAttr, "abc">:$dv_str_attr,
    BoolAttr:$bool_attr,
    DefaultValuedAttr<BoolAttr, "true">:$dv_bool_attr,
    SomeI32Enum:$enum_attr,
    DefaultValuedAttr<SomeI32Enum, "::SomeI32Enum::case5">:$dv_enum_attr
  );
}

// DECL-LABEL: EOp declarations
// DECL: static void build({{.*}}, uint32_t i32_attr, uint32_t dv_i32_attr, ::llvm::APFloat f64_attr, ::llvm::APFloat dv_f64_attr, ::llvm::StringRef str_attr, ::llvm::StringRef dv_str_attr, bool bool_attr, bool dv_bool_attr, ::SomeI32Enum enum_attr, ::SomeI32Enum dv_enum_attr = ::SomeI32Enum::case5)


// Test proper namespacing for AttrDef
// ---

def NamespaceOp : NS_Op<"namespace_op", []> {
  let arguments = (ins
      SomeAttrDef:$AttrDef
  );
}

// Test mixing operands and attributes in arbitrary order
// ---

def MixOperandsAndAttrs : NS_Op<"mix_operands_and_attrs", []> {
  let arguments = (ins F32Attr:$attr, F32:$operand, F32Attr:$otherAttr, F32:$otherArg);
}

// DECL-LABEL: MixOperandsAndAttrs declarations
// DECL-DAG: ::mlir::TypedValue<::mlir::FloatType> getOperand()
// DECL-DAG: ::mlir::TypedValue<::mlir::FloatType> getOtherArg()

// DECL-LABEL: NamespaceOp declarations
// DECL: foobar::SomeAttrAttr getAttrDef()


def OpWithDefaultAndRegion : NS_Op<"default_with_region", []> {
  let arguments = (ins
          DefaultValuedAttr<BoolAttr, "true">:$dv_bool_attr
  );
  let regions = (region VariadicRegion<AnyRegion>:$region);
}

// We should not have a default attribute in this case.

// DECL-LABEL: OpWithDefaultAndRegion declarations
// DECL: static void build({{.*}}, bool dv_bool_attr, unsigned regionCount)

def OpWithDefaultAndSuccessor : NS_Op<"default_with_succ", []> {
  let arguments = (ins
          DefaultValuedAttr<BoolAttr, "true">:$dv_bool_attr
  );
  let successors = (successor VariadicSuccessor<AnySuccessor>:$succ);
}

// We should not have a default attribute in this case.

// DECL-LABEL: OpWithDefaultAndSuccessor declarations
// DECL-DAG: static void build({{.*}}, bool dv_bool_attr, ::mlir::BlockRange succ)

// DEF-LABEL: MixOperandsAndAttrs definitions
// DEF-DAG: void MixOperandsAndAttrs::build(::mlir::OpBuilder &odsBuilder, ::mlir::OperationState &odsState, ::mlir::FloatAttr attr, ::mlir::Value operand, ::mlir::FloatAttr otherAttr, ::mlir::Value otherArg)
// DEF-DAG: ::llvm::APFloat MixOperandsAndAttrs::getAttr()
// DEF-DAG: ::llvm::APFloat MixOperandsAndAttrs::getOtherAttr()

// Test unit attributes.
// ---

def UnitAttrOp : NS_Op<"unit_attr_op", []> {
  let arguments = (ins UnitAttr:$attr);
}

// DEF-LABEL: UnitAttrOp definitions
// DEF: bool UnitAttrOp::getAttr() {
// DEF:   return {{.*}} != nullptr


// DEF: build(::mlir::OpBuilder &odsBuilder, ::mlir::OperationState &odsState, /*optional*/::mlir::UnitAttr attr)
// DEF: build(::mlir::OpBuilder &odsBuilder, ::mlir::OperationState &odsState, /*optional*/bool attr)

// DECL-LABEL: UnitAttrOp declarations
// DECL: ::mlir::Attribute removeAttrAttr() {
// DECL-NEXT:   auto attr = getProperties().attr;
// DECL-NEXT:   getProperties().attr = {};
// DECL-NEXT:   return attr;
// DECL: build(::mlir::OpBuilder &odsBuilder, ::mlir::OperationState &odsState, /*optional*/bool attr = false)


// Test elementAttr field of TypedArrayAttr.
// ---

def SomeTypedArrayAttr : TypedArrayAttrBase<SomeAttr, "SomeAttr array">;

// RECORD-LABEL: def SomeTypedArrayAttr
// RECORD: Attr elementAttr = SomeAttr;
