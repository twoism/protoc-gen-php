/**
 * PHP Protocol Buffer Generator Plugin for protoc
 * By Andrew Brampton (c) 2010,2016
 */

#include "php_generator.h"

PHPFileGenerator::PHPFileGenerator(io::Printer& printer,
                                   const FileDescriptor& file,
                                   const string& parameter)
    : printer_(printer),
      file_(file),
      parameter_(parameter),

      // Parse the file options
      options_(file.options().GetExtension(php)) {
  (void)parameter_;  // Unused
}

PHPFileGenerator::~PHPFileGenerator() {}

string PHPFileGenerator::DefaultValueAsString(const FieldDescriptor &field) {
  switch (field.cpp_type()) {
    case FieldDescriptor::CPPTYPE_INT32:
      return SimpleItoa(field.default_value_int32());

    case FieldDescriptor::CPPTYPE_INT64:
      return SimpleItoa(field.default_value_int64());

    case FieldDescriptor::CPPTYPE_UINT32:
      return SimpleItoa(field.default_value_uint32());

    case FieldDescriptor::CPPTYPE_UINT64:
      return SimpleItoa(field.default_value_uint64());

    case FieldDescriptor::CPPTYPE_FLOAT:
      return SimpleFtoa(field.default_value_float());

    case FieldDescriptor::CPPTYPE_DOUBLE:
      return SimpleDtoa(field.default_value_double());

    case FieldDescriptor::CPPTYPE_BOOL:
      return field.default_value_bool() ? "true" : "false";

    case FieldDescriptor::CPPTYPE_STRING:
      return "\"" + PHPEscape(field.default_value_string()) + "\"";

    case FieldDescriptor::CPPTYPE_ENUM:
      return ClassName(*field.enum_type()) + "::" + field.default_value_enum()->name();

    case FieldDescriptor::CPPTYPE_MESSAGE:
      return "null";
  }

  assert(false);  // Every field has a default type, we are missing one
}

void PHPFileGenerator::FieldVariables(const OneofDescriptor &oneof,
                    map<string, string> &variables) {
  variables["name"] = VariableName(oneof.name());  // TODO Is this a bug?
  variables["oneof_name"] = VariableName(oneof.name());
  variables["oneof_case"] = "_" + VariableName(oneof.name()) + "case";
  variables["oneof_capitalized_name"] =
      UnderscoresToCapitalizedCamelCase(oneof.name());
  variables["oneof_definition"] = OneLineDefinition(oneof.DebugString());
  variables["oneof_default"] = "self::NONE";
  variables["oneof_field"] = oneof.name();
}

// Returns a map of variables related to this field
void PHPFileGenerator::FieldVariables(const FieldDescriptor &field,
                    map<string, string> &variables) {
  variables.clear();

  variables["name"] = VariableName(field.name());
  variables["[]"] = field.is_repeated() ? "[]" : "";
  variables["definition"] = OneLineDefinition(field.DebugString());
  variables["default"] = DefaultValueAsString(field);
  variables["capitalized_name"] =
      UnderscoresToCapitalizedCamelCase(field.name());
  variables["field"] = field.name();

  if (field.containing_oneof()) {
    variables["oneof"] = OneOfConstant(field.name());
    FieldVariables(Deref(field.containing_oneof()), variables);
  }

  switch (field.type()) {
    //      TODO If PHP>7 enums should be type checked as a int
    //      case FieldDescriptor::TYPE_ENUM:
    //        variables["type"] = "int ";
    //        break;

    case FieldDescriptor::TYPE_MESSAGE:
    case FieldDescriptor::TYPE_GROUP:
      variables["type"] = ClassName(Deref(field.message_type())) + " ";
      break;

    default:
      variables["type"] = "";
  }
}


bool PHPFileGenerator::Generate(string* error) {
  // TODO Check if the last-mod time of the php file is newer than the proto. If
  // so skip.
  string php_filename = FileDescriptorToPath(file_);

  try {
    printer_.Print(
        "<?php\n"
        "// Please include protocolbuffers before this file, for example:\n"
        "//   require('protocolbuffers.inc.php');\n"
        "//   require('`filename`');\n",
        "filename", php_filename.c_str());

    printer_.Print("\n");

    if (UseNamespaces() && !file_.package().empty()) {
      // If we are using namespaces
      printer_.Print(
        "namespace `namespace` {\n"
        "  use Protobuf;\n"
        "  use ProtobufEnum;\n"
        "  use ProtobufMessage;\n",

        "namespace", NamespaceName(file_).c_str()); 
      printer_.Indent();
    }

    // TODO Wrap the following in option that asks about autoloading
    // TODO Move the following into a method
    for (int i = 0; i < file_.dependency_count(); i++) {
      const FileDescriptor& dep_file(Deref(file_.dependency(i)));

      printer_.Print("require('`filename`');\n",
        "filename", FileDescriptorToPath(dep_file).c_str());
    }

    PrintEnums();
    PrintMessages();
    PrintServices();

    if (UseNamespaces() && !file_.package().empty()) {
      printer_.Outdent();
      printer_.Print("}");
    }

  } catch (const std::string& msg) {
    *error = msg;
    return false;

  } catch (const char* msg) {
    error->assign(msg);
    return false;
  }

  return true;
}
