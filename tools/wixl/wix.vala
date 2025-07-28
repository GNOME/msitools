namespace Wixl {

    public interface WixResolver {
        public abstract G? find_element<G> (string Id);

        public G? resolve<G> (WixElement element) throws GLib.Error {
            G? resolved = null;

            if (element.get_type ().is_a (typeof (G)))
                resolved = element;
            else if (element is WixElementRef) {
                var ref = element as WixElementRef<G>;
                if (!ref.ref_type.is_a (typeof (G)))
                    resolved = null;
                else if (ref.resolved == null)
                    ref.resolved = find_element<G> (element.Id);
                resolved = ref.resolved;
            }

            if (resolved == null)
                throw new Wixl.Error.FAILED ("couldn't resolve %s", element.Id);

            return resolved;
        }
    }

    public enum VisitState {
        ENTER,
        INFIX,
        LEAVE
    }

    public abstract class WixNodeVisitor: Object {
        public abstract void visit_product (WixProduct product) throws GLib.Error;
        public abstract void visit_icon (WixIcon icon) throws GLib.Error;
        public abstract void visit_package (WixPackage package) throws GLib.Error;
        public abstract void visit_property (WixProperty prop) throws GLib.Error;
        public abstract void visit_media (WixMedia media) throws GLib.Error;
        public abstract void visit_directory (WixDirectory dir) throws GLib.Error;
        public abstract void visit_component (WixComponent comp, VisitState state) throws GLib.Error;
        public abstract void visit_feature (WixFeature feature, VisitState state) throws GLib.Error;
        public abstract void visit_component_ref (WixComponentRef ref) throws GLib.Error;
        public abstract void visit_remove_folder (WixRemoveFolder rm) throws GLib.Error;
        public abstract void visit_registry_value (WixRegistryValue reg) throws GLib.Error;
        public abstract void visit_file (WixFile reg) throws GLib.Error;
        public abstract void visit_remove_file (WixRemoveFile rf) throws GLib.Error;
        public abstract void visit_ini_file (WixIniFile reg) throws GLib.Error;
        public abstract void visit_shortcut (WixShortcut shortcut) throws GLib.Error;
        public abstract void visit_create_folder (WixCreateFolder folder) throws GLib.Error;
        public abstract void visit_fragment (WixFragment fragment) throws GLib.Error;
        public abstract void visit_sequence (WixSequence sequence) throws GLib.Error;
        public abstract void visit_condition (WixCondition condition) throws GLib.Error;
        public abstract void visit_upgrade (WixUpgrade upgrade) throws GLib.Error;
        public abstract void visit_upgrade_version (WixUpgradeVersion version) throws GLib.Error;
        public abstract void visit_action (WixAction action) throws GLib.Error;
        public abstract void visit_text (WixText text) throws GLib.Error;
        public abstract void visit_component_group_ref (WixComponentGroupRef ref) throws GLib.Error;
        public abstract void visit_progid (WixProgId progid) throws GLib.Error;
        public abstract void visit_extension (WixExtension extension) throws GLib.Error;
        public abstract void visit_verb (WixVerb verb) throws GLib.Error;
        public abstract void visit_mime (WixMIME mime) throws GLib.Error;
        public abstract void visit_service_argument (WixServiceArgument service_argument) throws GLib.Error;
        public abstract void visit_service_control (WixServiceControl service_control, VisitState state) throws GLib.Error;
        public abstract void visit_service_dependency (WixServiceDependency service_dependency) throws GLib.Error;
        public abstract void visit_service_install (WixServiceInstall service_install, VisitState state) throws GLib.Error;
        public abstract void visit_registry_search (WixRegistrySearch search) throws GLib.Error;
        public abstract void visit_custom_action (WixCustomAction action) throws GLib.Error;
        public abstract void visit_binary (WixBinary binary) throws GLib.Error;
        public abstract void visit_major_upgrade (WixMajorUpgrade major) throws GLib.Error;
        public abstract void visit_media_template (WixMediaTemplate media) throws GLib.Error;
        public abstract void visit_ui_ref (WixUIRef ref) throws GLib.Error;
        public abstract void visit_ui_text (WixUIText text) throws GLib.Error;
        public abstract void visit_text_element (WixTextElement text) throws GLib.Error;
        public abstract void visit_text_style (WixTextStyle style) throws GLib.Error;
        public abstract void visit_dialog (WixDialog dialog) throws GLib.Error;
        public abstract void visit_dialog_ref (WixDialogRef ref) throws GLib.Error;
        public abstract void visit_control (WixControl control) throws GLib.Error;
        public abstract void visit_publish (WixPublish publish) throws GLib.Error;
        public abstract void visit_radio_button (WixRadioButton radio) throws GLib.Error;
        public abstract void visit_subscribe (WixSubscribe subscribe) throws GLib.Error;
        public abstract void visit_progress_text (WixProgressText progress_text) throws GLib.Error;
        public abstract void visit_environment (WixEnvironment env) throws GLib.Error;
        public abstract void visit_copy_file (WixCopyFile copy_file) throws GLib.Error;
    }

    public abstract class WixNode: Object {
        public WixElement? parent;

        static construct {
            Value.register_transform_func (typeof (WixNode), typeof (string), (ValueTransform)WixNode.value_to_string);
        }

        public abstract string to_string ();

        public static void value_to_string (Value src, out Value dest) {
            WixNode e = value_get_node (src);

            dest = e.to_string ();
        }

        public static WixNode? value_get_node (Value value) {
            if (! value.holds (typeof (WixNode)))
                return null;

            return (WixNode)value.get_object ();
        }

        public abstract void accept (WixNodeVisitor visitor) throws GLib.Error;
    }

    public class WixText: WixNode {
        public string Text;

        public WixText (string str) {
            Text = str;
        }

        public override string to_string () {
            return Text;
        }

        public override void accept (WixNodeVisitor visitor) throws GLib.Error {
            visitor.visit_text (this);
        }
    }

    public abstract class WixElement: WixNode {
        public class string name;

        public string Id { get; set; }
        public List<WixNode> children;

        // FIXME: would be nice if vala always initialize class member to null
        // GObject copy class init so other class hashtable will be unrefed...??
        protected class HashTable<string, Type> *child_types = null;
        class construct {
            child_types = new HashTable<string, Type> (str_hash, str_equal);
        }

        public class void add_child_types (HashTable<string, Type> table, Type[] child_types) {
            foreach (var t in child_types) {
                var n = ((WixElement) Object.new (t)).name;
                table.insert (n, t);
            }
        }

        public void add_child (WixNode e) {
            e.parent = this;
            children.append (e);
        }

        public G[] add_elements<G> (owned G[] a) {
            // jeez, vala, took me a while to workaround generics & array issues..
            var array = a;
            var type = typeof (G);

            if (this.get_type ().is_a (type))
                array += this;

            foreach (var c in children) {
                if (c is WixElement)
                    array = ((WixElement)c).add_elements<G> (array);
                else if (c.get_type ().is_a (type))
                    array += c;
            }

            return array;
        }

        public virtual string full_path (WixResolver r) throws GLib.Error {
            if (parent != null && (parent is WixDirectory || parent is WixDirectoryRef))
                return parent.full_path (r) + "/" + this.Id;
            else
                return this.Id;
        }

        public G[] get_elements<G> () {
            return add_elements<G> ({});
        }

        public G? find_element<G> (string Id) {
            var type = typeof (G);
            if (this.Id == Id && this.get_type () == type)
                return this;

            foreach (var c in children) {
                if (c is WixElement) {
                    var e = ((WixElement)c).find_element<G> (Id);
                    if (e != null)
                        return e;
                }
            }

            return null;
        }

        string get_attribute_content (Xml.Attr *attr) {
            if (attr->children == null)
                return "";

            return attr->children->content;
        }

        protected void load_properties_from_node (Xml.Node *node) throws Wixl.Error {
            for (var prop = node->properties; prop != null; prop = prop->next) {
                if (prop->type == Xml.ElementType.ATTRIBUTE_NODE)
                    set_property (prop->name, get_attribute_content (prop));
            }
        }

        public virtual void load (Xml.Node *node) throws Wixl.Error {
            if (name != null && node->name != name)
                throw new Error.FAILED ("%s: invalid node %s".printf (name, node->name));

            load_properties_from_node (node);
            for (var child = node->children; child != null; child = child->next) {
                switch (child->type) {
                case Xml.ElementType.COMMENT_NODE:
                    continue;
                case Xml.ElementType.TEXT_NODE:
                    add_child (new WixText (child->content));
                    continue;
                case Xml.ElementType.ELEMENT_NODE:
                    var t = child_types->lookup (child->name);
                    if (t != 0) {
                        var elem = Object.new (t) as WixElement;
                        elem.load (child);
                        add_child (elem);
                        continue;
                    }
                    break;
                default:
                    break;
                }
                error ("unhandled child %s node %s", name, child->name);
            }
        }

        public override string to_string () {
            var type = get_type ();
            var klass = (ObjectClass)type.class_ref ();
            var str = "<" + name;

            var i = 0;
            foreach (var p in klass.list_properties ()) {
                if (!(ParamFlags.READABLE in p.flags))
                    continue;
                var value = Value (p.value_type);
                get_property (p.name, ref value);
                var valstr = value.holds (typeof (string)) ?
                    (string)value : value.strdup_contents ();
                if (valstr != null)
                    str += " " + p.name + "=\"" + valstr + "\"";
                i += 1;
            }

            if (children.length () != 0) {
                str += ">\n";

                foreach (var child in children) {
                    str += child.to_string () + "\n";
                }

                return str + "</" + name + ">";
            } else
                return str + "/>";
        }

        public override void accept (WixNodeVisitor visitor) throws GLib.Error {
            foreach (var child in children)
                child.accept (visitor);
        }
    }

    public class WixComponentGroup: WixElement {
        static construct {
            name = "ComponentGroup";

            add_child_types (child_types, {
                typeof (WixComponent),
                typeof (WixComponentRef),
                typeof (WixComponentGroupRef),
            });
        }

        public string Directory { get; set; }

    }

    public class WixFragment: WixElement {
        static construct {
            name = "Fragment";

            add_child_types (child_types, {
                typeof (WixDirectory),
                typeof (WixDirectoryRef),
                typeof (WixComponentGroup),
                typeof (WixBinary),
                typeof (WixUI),
                typeof (WixUIRef),
                typeof (WixInstallUISequence),
            });
        }

        public override void accept (WixNodeVisitor visitor) throws GLib.Error {
            base.accept (visitor);
            visitor.visit_fragment (this);
        }
    }

    public class WixRegistrySearch: WixElement {
        static construct {
            name = "RegistrySearch";
        }

        public string Root { get; set; }
        public string Key { get; set; }
        public string Type { get; set; }
        public string Name { get; set; }
        public string? Win64 { get; set; }

        public override void accept (WixNodeVisitor visitor) throws GLib.Error {
            visitor.visit_registry_search (this);
        }
    }

    public class WixProperty: WixElement {
        static construct {
            name = "Property";

            add_child_types (child_types, {
                typeof (WixRegistrySearch),
            });
        }

        public string Value { get; set; }
        public string Secure { get; set; }

        public override void accept (WixNodeVisitor visitor) throws GLib.Error {
            base.accept (visitor);
            visitor.visit_property (this);
        }
    }

    public class WixPackage: WixElement {
        static construct {
            name = "Package";
        }

        public string Keywords { get; set; }
        public string InstallerDescription { get; set; }
        public string InstallerComments { get; set; }
        public string Manufacturer { get; set; }
        public string InstallerVersion { get; set; }
        public string Languages { get; set; }
        public string Compressed { get; set; }
        public string SummaryCodepage { get; set; }
        public string Comments { get; set; }
        public string Description { get; set; }
        public string InstallScope { get; set; }

        public override void accept (WixNodeVisitor visitor) throws GLib.Error {
            base.accept (visitor);
            visitor.visit_package (this);
        }
    }

    public class WixCreateFolder: WixElement {
        static construct {
            name = "CreateFolder";
        }

        public string Directory { get; set; }

        public override void accept (WixNodeVisitor visitor) throws GLib.Error {
            visitor.visit_create_folder (this);
        }
    }

    public class WixIcon: WixElement {
        static construct {
            name = "Icon";
        }

        public string SourceFile { get; set; }

        public File file;

        public override void accept (WixNodeVisitor visitor) throws GLib.Error {
            visitor.visit_icon (this);
        }
    }

    public class WixShortcut: WixElement {
        static construct {
            name = "Shortcut";
        }

        public string Directory { get; set; }
        public string Name { get; set; }
        public string IconIndex { get; set; }
        public string WorkingDirectory { get; set; }
        public string Icon { get; set; }
        public string Advertise { get; set; }
        public string Arguments { get; set; }
        public string Description { get; set; }
        public string Target { get; set; }

        public Libmsi.Record record;

        public WixComponent? get_component () {
            if (parent is WixFile || parent is WixCreateFolder)
                return parent.parent as WixComponent;
            else
                return parent as WixComponent;
        }

        public override void accept (WixNodeVisitor visitor) throws GLib.Error {
            visitor.visit_shortcut (this);
        }
    }

    public abstract class WixKeyElement: WixElement {
        public string KeyPath { get; set; }

        public virtual string path_name () throws GLib.Error {
            throw new Wixl.Error.FAILED("this key path does not support generating a component GUID");
        }
    }

    public class WixFile: WixKeyElement {
        static construct {
            name = "File";

            add_child_types (child_types, {
                typeof (WixShortcut),
                typeof (WixCopyFile)
            });
        }

        public string DiskId { get; set; }
        public string Source { get; set; }
        public string Name { get; set; }
        public string DefaultVersion { get; set; }
        public string CompanionFile { get; set; }

        public File file;

        public override string path_name () throws GLib.Error {
            return Name ?? Path.get_basename (Source);
        }

        public override void accept (WixNodeVisitor visitor) throws GLib.Error {
            base.accept (visitor);
            visitor.visit_file (this);
        }
    }

    public class WixIniFile: WixElement {
        static construct {
            name = "IniFile";
        }

        public string Action { get; set; }
        public string Directory { get; set; }
        public string Key { get; set; }
        public string Name { get; set; }
        public string Section { get; set; }
        public string Value { get; set; }


        public override void accept (WixNodeVisitor visitor) throws GLib.Error {
            visitor.visit_ini_file (this);
        }
    }

    public class WixRegistryValue: WixKeyElement {
        static construct {
            name = "RegistryValue";
        }

        public string Root { get; set; }
        public string Key { get; set; }
        public string Type { get; set; }
        public string Value { get; set; }
        public string Name { get; set; }

        public override string path_name () throws GLib.Error {
            return Root + "/" + Key;
        }

        public override void accept (WixNodeVisitor visitor) throws GLib.Error {
            visitor.visit_registry_value (this);
        }
    }

    public class WixRemoveFolder: WixElement {
        static construct {
            name = "RemoveFolder";
        }

        public string On { get; set; }

        public override void accept (WixNodeVisitor visitor) throws GLib.Error {
            visitor.visit_remove_folder (this);
        }
    }

    public class WixRemoveFile: WixElement {
        static construct {
            name = "RemoveFile";
        }

        public string Name { get; set; }
        public string On { get; set; }

        public override void accept (WixNodeVisitor visitor) throws GLib.Error {
            visitor.visit_remove_file (this);
        }
    }

    public class WixFeature: WixElement {
        static construct {
            name = "Feature";

            add_child_types (child_types, {
                typeof (WixComponentRef),
                typeof (WixComponentGroupRef),
                typeof (WixFeature),
                typeof (WixCondition),
            });
        }

        public string Level { get; set; }
        public string Title { get; set; }
        public string Description { get; set; }
        public string Display { get; set; }
        public string ConfigurableDirectory { get; set; }
        public string AllowAdvertise { get; set; }
        public string Absent { get; set; }
        public string InstallDefault { get; set; }
        public string TypicalDefault { get; set; }

        public override void accept (WixNodeVisitor visitor) throws GLib.Error {
            visitor.visit_feature (this, VisitState.ENTER);
            base.accept (visitor);
            visitor.visit_feature (this, VisitState.LEAVE);
        }
    }

    public class WixComponentRef: WixElementRef<WixComponent> {
        static construct {
            name = "ComponentRef";
            ref_type = typeof (WixComponent);
        }

        public override string full_path (WixResolver r) throws GLib.Error {
            return ((WixElement)r.resolve<WixComponent> (this)).full_path (r);
        }

        public override void accept (WixNodeVisitor visitor) throws GLib.Error {
            visitor.visit_component_ref (this);
        }
    }

    public class WixComponentGroupRef: WixElementRef<WixComponentGroup> {
        static construct {
            name = "ComponentGroupRef";
            ref_type = typeof (WixComponentGroup);
        }

        public override string full_path (WixResolver r) throws GLib.Error {
            return ((WixElement)r.resolve<WixComponentGroup> (this)).full_path (r);
        }

        public override void accept (WixNodeVisitor visitor) throws GLib.Error {
            visitor.visit_component_group_ref (this);
        }
    }

    public class WixCondition: WixElement {
        static construct {
            name = "Condition";
        }

        public string Message { get; set; }
        public string Level { get; set; }

        public override void accept (WixNodeVisitor visitor) throws GLib.Error {
            visitor.visit_condition (this);
        }
    }

    public class WixMIME: WixElement {
        static construct {
            name = "MIME";
        }

        public string Advertise { get; set; }
        public string ContentType { get; set; }
        public string Default { get; set; }

        public override void accept (WixNodeVisitor visitor) throws GLib.Error {
            visitor.visit_mime (this);
        }
    }

    public class WixServiceArgument: WixElement {
        static construct {
            name = "ServiceArgument";
        }

        public string get_text() {
            foreach (var c in children) {
                if (c is WixText) {
                    return ((WixText)c).Text;
                }
            }

            return "";
        }

        public override void accept (WixNodeVisitor visitor) throws GLib.Error {
            visitor.visit_service_argument (this);
        }
    }

    public class WixServiceControl: WixElement {
        static construct {
            name = "ServiceControl";

            add_child_types (child_types, {
                typeof (WixServiceArgument),
            });
        }

        public string Name { get; set; }
        public string Start { get; set; }
        public string Stop { get; set; }
        public string Remove { get; set; }
        public string Wait { get; set; }

        public override void accept (WixNodeVisitor visitor) throws GLib.Error {
            visitor.visit_service_control (this, VisitState.ENTER);
            base.accept (visitor);
            visitor.visit_service_control (this, VisitState.LEAVE);
        }
    }

    public class WixServiceDependency: WixElement {
        static construct {
            name = "ServiceDependency";
        }

        public string Group { get; set; }

        public override void accept (WixNodeVisitor visitor) throws GLib.Error {
            visitor.visit_service_dependency (this);
        }
    }

    public class WixServiceInstall: WixElement {
        static construct {
            name = "ServiceInstall";

            add_child_types (child_types, {
                typeof (WixServiceDependency),
            });
        }

        public string Name { get; set; }
        public string DisplayName { get; set; }
        public string Type { get; set; }
        public string Interactive { get; set; }
        public string Start { get; set; }
        public string ErrorControl { get; set; }
        public string Vital { get; set; }
        public string LoadOrderGroup { get; set; }
        public string Account { get; set; }
        public string Password { get; set; }
        public string Arguments { get; set; }
        public string Description { get; set; }
        public string EraseDescription { get; set; }

        public override void accept (WixNodeVisitor visitor) throws GLib.Error {
            visitor.visit_service_install (this, VisitState.ENTER);
            base.accept (visitor);
            visitor.visit_service_install (this, VisitState.LEAVE);
        }
    }

    public class WixVerb: WixElement {
        static construct {
            name = "Verb";
        }

        public string Command { get; set; }
        public string TargetFile { get; set; }
        public string Argument { get; set; }

        public override void accept (WixNodeVisitor visitor) throws GLib.Error {
            base.accept (visitor);
            visitor.visit_verb (this);
        }
    }

    public class WixExtension: WixElement {
        static construct {
            name = "Extension";

            add_child_types (child_types, {
                typeof (WixVerb),
                typeof (WixMIME),
            });
        }

        public string ContentType { get; set; }
        public string Advertise { get; set; }

        public override void accept (WixNodeVisitor visitor) throws GLib.Error {
            base.accept (visitor);
            visitor.visit_extension (this);
        }
    }

    public class WixProgId: WixElement {
        static construct {
            name = "ProgId";

            add_child_types (child_types, {
                typeof (WixExtension),
            });
        }

        public string Description { get; set; }
        public string Advertise { get; set; }
        public string? Icon { get; set; }
        public string? IconIndex { get; set; }

        public override void accept (WixNodeVisitor visitor) throws GLib.Error {
            base.accept (visitor);
            visitor.visit_progid (this);
        }
    }

    public class WixAction: WixElement {
        public new string name;

        public string After { get; set; }
        public string Before { get; set; }
        public string Overridable { get; set; }
        public string Sequence { get; set; }
        public string Suppress { get; set; }
        public string Action { get; set; }
        public string Dialog { get; set; }
        public string OnExit { get; set; }

        // not in the specification, but used by layouts?
        public string Condition { get; set; }

        public override void load (Xml.Node *node) throws Wixl.Error {
            base.load (node);
            name = node->name;
        }

        public override void accept (WixNodeVisitor visitor) throws GLib.Error {
            visitor.visit_action (this);
        }
    }

    public class WixSequence: WixElement {
        public override void accept (WixNodeVisitor visitor) throws GLib.Error {
            base.accept (visitor);
            visitor.visit_sequence (this);
        }
    }

    public class WixInstallExecuteSequence: WixSequence {
        static construct {
            name = "InstallExecuteSequence";

            foreach (var action in new string[] {
                    "AllocateRegistrySpace",
                    "AppSearch",
                    "BindImage",
                    "CCPSearch",
                    "CostFinalize",
                    "CostInitialize",
                    "CreateFolders",
                    "CreateShortcuts",
                    "Custom",
                    "DeleteServices",
                    "DisableRollback",
                    "DuplicateFiles",
                    "FileCost",
                    "FindRelatedProducts",
                    "ForceReboot",
                    "InstallExecute",
                    "InstallExecuteAgain",
                    "InstallFiles",
                    "InstallFinalize",
                    "InstallInitialize",
                    "InstallODBC",
                    "InstallServices",
                    "InstallValidate",
                    "IsolateComponents",
                    "LaunchConditions",
                    "MigrateFeatureStates",
                    "MoveFiles",
                    "MsiPublishAssemblies",
                    "MsiUnpublishAssemblies",
                    "PatchFiles",
                    "ProcessComponents",
                    "PublishComponents",
                    "PublishFeatures",
                    "PublishProduct",
                    "RegisterClassInfo",
                    "RegisterComPlus",
                    "RegisterExtensionInfo",
                    "RegisterFonts",
                    "RegisterMIMEInfo",
                    "RegisterProduct",
                    "RegisterProgIdInfo",
                    "RegisterTypeLibraries",
                    "RegisterUser",
                    "RemoveDuplicateFiles",
                    "RemoveEnvironmentStrings",
                    "RemoveExistingProducts",
                    "RemoveFiles",
                    "RemoveFolders",
                    "RemoveIniValues",
                    "RemoveODBC",
                    "RemoveRegistryValues",
                    "RemoveShortcuts",
                    "ResolveSource",
                    "RMCCPSearch",
                    "ScheduleReboot",
                    "SelfRegModules",
                    "SelfUnregModules",
                    "SetODBCFolders",
                    "StartServices",
                    "StopServices",
                    "UnpublishComponents",
                    "UnpublishFeatures",
                    "UnregisterClassInfo",
                    "UnregisterComPlus",
                    "UnregisterExtensionInfo",
                    "UnregisterFonts",
                    "UnregisterMIMEInfo",
                    "UnregisterProgIdInfo",
                    "UnregisterTypeLibraries",
                    "ValidateProductID",
                    "WriteEnvironmentStrings",
                    "WriteIniValues",
                    "WriteRegistryValues" })
                child_types->insert (action, typeof (WixAction));
        }
    }

    public class WixInstallUISequence: WixSequence {
        static construct {
            name = "InstallUISequence";

            foreach (var action in new string[] {
                    "AppSearch",
                    "CCPSearch",
                    "CostFinalize",
                    "CostInitialize",
                    "Custom",
                    "ExecuteAction",
                    "ExitDialog",
                    "FatalError",
                    "FileCost",
                    "FindRelatedProducts",
                    "IsolateComponents",
                    "LaunchConditions",
                    "MaintenanceWelcomeDlg",
                    "MigrateFeatureStates",
                    "PrepareDlg",
                    "ProgressDlg",
                    "ResolveSource",
                    "ResumeDlg",
                    "RMCCPSearch",
                    "ScheduleReboot",
                    "Show",
                    "UserExit",
                    "ValidateProductID",
                    "WelcomeDlg",
                    "WelcomeEulaDlg" })
                child_types->insert (action, typeof (WixAction));
        }
    }

    public class WixAdminExecuteSequence: WixSequence {
        static construct {
            name = "AdminExecuteSequence";

            foreach (var action in new string[] {
                    "CostFinalize",
                    "CostInitialize",
                    "Custom",
                    "FileCost",
                    "InstallAdminPackage",
                    "InstallFiles",
                    "InstallFinalize",
                    "InstallInitialize",
                    "InstallValidate",
                    "LaunchConditions",
                    "PatchFiles",
                    "ResolveSource" })
                child_types->insert (action, typeof (WixAction));
        }
    }

    public class WixAdminUISequence: WixSequence {
        static construct {
            name = "AdminUISequence";

            foreach (var action in new string[] {
                    "CostFinalize",
                    "CostInitialize",
                    "Custom",
                    "ExecuteAction",
                    "ExitDialog",
                    "FatalError",
                    "FileCost",
                    "InstallAdminPackage",
                    "InstallFiles",
                    "InstallFinalize",
                    "InstallInitialize",
                    "InstallValidate",
                    "LaunchConditions",
                    "Show",
                    "UserExit" })
                child_types->insert (action, typeof (WixAction));
        }
    }

    public class WixAdvertiseExecuteSequence: WixSequence {
        static construct {
            name = "AdvertiseExecuteSequence";

            foreach (var action in new string[] {
                    "CostFinalize",
                    "CostInitialize",
                    "CreateShortcuts",
                    "Custom",
                    "InstallFinalize",
                    "InstallInitialize",
                    "InstallValidate",
                    "MsiPublishAssemblies",
                    "PublishComponents",
                    "PublishFeatures",
                    "PublishProduct",
                    "RegisterClassInfo",
                    "RegisterExtensionInfo",
                    "RegisterMIMEInfo",
                    "RegisterProgIdInfo" })
                child_types->insert (action, typeof (WixAction));
        }
    }

    public class WixUpgrade: WixElement {
        static construct {
            name = "Upgrade";

            add_child_types (child_types, {
                typeof (WixUpgradeVersion),
            });
        }

        public override void accept (WixNodeVisitor visitor) throws GLib.Error {
            base.accept (visitor);
            visitor.visit_upgrade (this);
        }
    }

    public class WixUpgradeVersion: WixElement {
        static construct {
            name = "UpgradeVersion";
        }

        public string Minimum { get; set; }
        public string Maximum { get; set; }
        public string IncludeMinimum { get; set; }
        public string IncludeMaximum { get; set; }
        public string MigrateFeatures { get; set; }
        public string OnlyDetect { get; set; }
        public string Property { get; set; }

        public override void accept (WixNodeVisitor visitor) throws GLib.Error {
            visitor.visit_upgrade_version (this);
        }
    }

    public class WixCustomAction: WixElement {
        static construct {
            name = "CustomAction";
        }

        public string Property { get; set; }
        public string Execute { get; set; }
        public string FileKey { get; set; }
        public string ExeCommand { get; set; }
        public string Impersonate { get; set; }
        public string Return { get; set; }
        public string BinaryKey { get; set; }
        public string DllEntry { get; set; }
        public string HideTarget { get; set; }
        public string JScriptCall { get; set; }
        public string Value { get; set; }

        public override void accept (WixNodeVisitor visitor) throws GLib.Error {
            visitor.visit_custom_action (this);
        }
    }

    public class WixBinary: WixElement {
        static construct {
            name = "Binary";
        }

        public string SourceFile { get; set; }

        public File file;

        public override void accept (WixNodeVisitor visitor) throws GLib.Error {
            visitor.visit_binary (this);
        }
    }

    public class WixMajorUpgrade: WixElement {
        static construct {
            name = "MajorUpgrade";
        }

        public string DowngradeErrorMessage { get; set; }

        public string AllowSameVersionUpgrades { get; set; }

        public string AllowDowngrades { get; set; }

        public override void accept (WixNodeVisitor visitor) throws GLib.Error {
            visitor.visit_major_upgrade (this);
        }
    }

    public class WixMediaTemplate: WixElement {
        static construct {
            name = "MediaTemplate";
        }

        public string EmbedCab { get; set; }

        public override void accept (WixNodeVisitor visitor) throws GLib.Error {
            visitor.visit_media_template (this);
        }
    }

    public class WixProduct: WixElement {
        static construct {
            name = "Product";

            add_child_types (child_types, {
                typeof (WixComponentGroup),
                typeof (WixCondition),
                typeof (WixDirectory),
                typeof (WixDirectoryRef),
                typeof (WixFeature),
                typeof (WixIcon),
                typeof (WixInstallExecuteSequence),
                typeof (WixInstallUISequence),
                typeof (WixAdminExecuteSequence),
                typeof (WixAdminUISequence),
                typeof (WixAdvertiseExecuteSequence),
                typeof (WixMedia),
                typeof (WixPackage),
                typeof (WixProperty),
                typeof (WixUpgrade),
                typeof (WixCustomAction),
                typeof (WixBinary),
                typeof (WixMajorUpgrade),
                typeof (WixMediaTemplate),
                typeof (WixUIRef),
            });
        }

        public string Name { get; set; }
        public string UpgradeCode { get; set; }
        public string Language { get; set; }
        public string Codepage { get; set; }
        public string Version { get; set; }
        public string Manufacturer { get; set; }

        public WixProduct () {
        }

        public override void accept (WixNodeVisitor visitor) throws GLib.Error {
            base.accept (visitor);
            visitor.visit_product (this);
        }
    }

    public class WixMedia: WixElement {
        static construct {
            name = "Media";
        }

        public string Cabinet { get; set; }
        public string EmbedCab { get; set; }
        public string DiskPrompt { get; set; }

        public Libmsi.Record record;

        public override void accept (WixNodeVisitor visitor) throws GLib.Error {
            visitor.visit_media (this);
        }
    }

    public class WixRegistryKey: WixElement {
        static construct {
            name = "RegistryKey";

            add_child_types (child_types, {
                typeof (WixRegistryValue),
            });
        }

        public string Key { get; set; }
        public string Root { get; set; }
    }

    public class WixComponent: WixElement {
        static construct {
            name = "Component";

            add_child_types (child_types, {
                typeof (WixCreateFolder),
                typeof (WixRemoveFolder),
                typeof (WixRegistryValue),
                typeof (WixFile),
                typeof (WixRemoveFile),
                typeof (WixShortcut),
                typeof (WixProgId),
                typeof (WixRegistryKey),
                typeof (WixServiceControl),
                typeof (WixServiceInstall),
                typeof (WixIniFile),
                typeof (WixCondition),
                typeof (WixEnvironment),
                typeof (WixCopyFile)
            });
        }

        public string Guid { get; set; }
        public string? Win64 { get; set; }
        public WixKeyElement? key;
        public string Permanent { get; set; }
        public string Condition { get; set; }

        public List<WixFeature> in_feature;

        public override void accept (WixNodeVisitor visitor) throws GLib.Error {
            visitor.visit_component (this, VisitState.ENTER);
            base.accept (visitor);
            visitor.visit_component (this, VisitState.LEAVE);
        }

        public override string full_path (WixResolver r) throws GLib.Error {
            if (key == null)
                throw new Wixl.Error.FAILED("a child is needed to generate a component GUID");

            return base.full_path (r) + "/" + key.path_name ();
        }

    }

    public class WixDirectory: WixElement {
        static construct {
            name = "Directory";

            add_child_types (child_types, {
                typeof (WixDirectory),
                typeof (WixComponent),
            });
        }

        public string Name { get; set; }

        public override void accept (WixNodeVisitor visitor) throws GLib.Error {
            base.accept (visitor);
            visitor.visit_directory (this);
        }

        public string path_name () {
            return Name ?? Id;
        }

        public override string full_path (WixResolver r) throws GLib.Error {
            if (parent != null && (parent is WixDirectory || parent is WixDirectoryRef))
                return parent.full_path (r) + "/" + path_name ();
            else
                return path_name ();
        }
    }

    public class WixElementRef<G>: WixElement {
        public class Type ref_type;
        public G? resolved;

        // protected WixElementRef () {
        //     // FIXME vala: class init/construct fails, construct fails...
        //     ref_type = typeof (G);
        // }

        // FIXME vala: G type doesn't seem to be set correctly...
        public override string full_path (WixResolver r) throws GLib.Error {
            return ((WixElement)r.resolve<G> (this)).full_path (r);
        }
    }

    public class WixDirectoryRef: WixElementRef<WixDirectory> {
        static construct {
            name = "DirectoryRef";
            ref_type = typeof (WixDirectory);

            add_child_types (child_types, {
                typeof (WixDirectory),
                typeof (WixComponent),
            });
        }

        public override string full_path (WixResolver r) throws GLib.Error {
            return ((WixElement)r.resolve<WixDirectory> (this)).full_path (r);
        }
    }

    public class WixUI: WixElement {
        static construct {
            name = "UI";

            add_child_types (child_types, {
                typeof (WixBinary),
                typeof (WixDialog),
                typeof (WixDialogRef),
                typeof (WixProperty),
                typeof (WixPublish),
                typeof (WixTextStyle),
                typeof (WixUIRef),
                typeof (WixUIText),
                typeof (WixAdminUISequence),
                typeof (WixInstallUISequence),
                typeof (WixProgressText),
            });
        }
    }

    public class WixUIRef: WixElementRef<WixUI> {
        static construct {
            name = "UIRef";
            ref_type = typeof (WixUI);
        }

        public override string full_path (WixResolver r) throws GLib.Error {
            return ((WixElement)r.resolve<WixUI> (this)).full_path (r);
        }

        public override void accept (WixNodeVisitor visitor) throws GLib.Error {
            visitor.visit_ui_ref (this);
        }
    }

    public class WixUIText: WixElement {
        static construct {
            name = "UIText";
        }

        public string Value { get; set; }

        public override void accept (WixNodeVisitor visitor) throws GLib.Error {
            visitor.visit_ui_text (this);
        }
    }

    // note that this is different to the WixText node type
    public class WixTextElement: WixElement {
        static construct {
            name = "Text";
        }

        public string SourceFile { get; set; }

        public override void accept (WixNodeVisitor visitor) throws GLib.Error {
            visitor.visit_text_element (this);
        }
    }

    public class WixTextStyle: WixElement {
        static construct {
            name = "TextStyle";
        }

        public string Blue { get; set; }
        public string Bold { get; set; }
        public string FaceName { get; set; }
        public string Green { get; set; }
        public string Italic { get; set; }
        public string Red { get; set; }
        public string Size { get; set; }
        public string Strike { get; set; }
        public string Underline { get; set; }

        public override void accept (WixNodeVisitor visitor) throws GLib.Error {
            visitor.visit_text_style (this);
        }
    }

    public class WixDialog: WixElement {
        static construct {
            name = "Dialog";

            add_child_types (child_types, {
                typeof (WixControl),
            });
        }

        public string CustomPalette { get; set; }
        public string ErrorDialog { get; set; }
        public string Height { get; set; }
        public string Hidden { get; set; }
        public string KeepModeless { get; set; }
        public string LeftScroll { get; set; }
        public string Modeless { get; set; }
        public string NoMinimize { get; set; }
        public string RightAligned { get; set; }
        public string RightToLeft { get; set; }
        public string Title { get; set; }
        public string TrackDiskSpace { get; set; }
        public string Width { get; set; }

        // child controls can set themselves as the default or cancel action
        public string Default;
        public string Cancel;

        public override void accept (WixNodeVisitor visitor) throws GLib.Error {
            base.accept (visitor);
            visitor.visit_dialog (this);
        }
    }

    public class WixDialogRef: WixElementRef<WixDialog> {
        static construct {
            name = "DialogRef";
            ref_type = typeof (WixDialog);
        }

        public override string full_path (WixResolver r) throws GLib.Error {
            return ((WixElement)r.resolve<WixDialog> (this)).full_path (r);
        }

        public override void accept (WixNodeVisitor visitor) throws GLib.Error {
            visitor.visit_dialog_ref (this);
        }
    }

    public class WixControl: WixElement {
        static construct {
            name = "Control";

            add_child_types (child_types, {
                typeof (WixPublish),
                typeof (WixRadioButtonGroup),
                typeof (WixSubscribe),
                typeof (WixTextElement),
            });
        }

        public string Bitmap { get; set; }
        public string Cancel { get; set; }
        public string CheckBoxValue { get; set; }
        public string Default { get; set; }
        public string Disabled { get; set; }
        public string ElevationShield { get; set; }
        public string Fixed { get; set; }
        public string FixedSize { get; set; }
        public string Height { get; set; }
        public string Hidden { get; set; }
        public string IconSize { get; set; }
        public string NoPrefix { get; set; }
        public string ProgressBlocks { get; set; }
        public string Property { get; set; }
        public string Remote { get; set; }
        public string ShowRollbackCost { get; set; }
        public string Sunken { get; set; }
        public string Indirect { get; set; }
        public string Icon { get; set; }
        public string TabSkip { get; set; }
        public string Text { get; set; }
        public string ToolTip { get; set; }
        public string Transparent { get; set; }
        public string Type { get; set; }
        public string Width { get; set; }
        public string X { get; set; }
        public string Y { get; set; }

        // the specification suggests these should be in a child <Condition>
        // element but they are used as attributes in all the layouts
        public string DefaultCondition { get; set; }
        public string DisableCondition { get; set; }
        public string EnableCondition { get; set; }
        public string HideCondition { get; set; }
        public string ShowCondition { get; set; }

        // the text attribute can be populated from a child <Text>, which
        // itself could come from a file
        public File file;

        // updating the tab order involves a later pass through all the
        // controls, so keep a reference to the record so it can be updated
        public Libmsi.Record record;

        public override void accept (WixNodeVisitor visitor) throws GLib.Error {
            base.accept (visitor);
            visitor.visit_control (this);
        }
    }

    public class WixPublish: WixElement {
        static construct {
            name = "Publish";
        }

        // if it isn't set then the default value should be one greater than
        // any previous <Publish> element's order, so keep track
        public static int order = 1;

        public string Control { get; set; }
        public string Dialog { get; set; }
        public string Event { get; set; }
        public string Order { get; set; }
        public string Property { get; set; }
        public string Value { get; set; }

        // the specification suggests this should be the inner text but it's
        // also used as an attribute in some of the layouts
        public string Condition { get; set; }

        public override void accept (WixNodeVisitor visitor) throws GLib.Error {
            visitor.visit_publish (this);
        }
    }

    public class WixRadioButtonGroup: WixElement {
        static construct {
            name = "RadioButtonGroup";

            add_child_types (child_types, {
                typeof (WixRadioButton),
            });
        }

        public string Property { get; set; }
        public int order = 1;
    }

    public class WixRadioButton: WixElement {
        static construct {
            name = "RadioButton";
        }

        public string Height { get; set; }
        public string Help { get; set; }
        public string Text { get; set; }
        public string Value { get; set; }
        public string Width { get; set; }
        public string X { get; set; }
        public string Y { get; set; }

        public override void accept (WixNodeVisitor visitor) throws GLib.Error {
            visitor.visit_radio_button (this);
        }
    }

    public class WixSubscribe: WixElement {
        static construct {
            name = "Subscribe";
        }

        public string Attribute { get; set; }
        public string Event { get; set; }

        public override void accept (WixNodeVisitor visitor) throws GLib.Error {
            visitor.visit_subscribe (this);
        }
    }

    public class WixProgressText: WixElement {
        static construct {
            name = "ProgressText";
        }

        public string Action { get; set; }
        public string Template { get; set; }

        public override void accept (WixNodeVisitor visitor) throws GLib.Error {
            visitor.visit_progress_text (this);
        }
    }

    class WixRoot: WixElement {
        static construct {
            name = "Wix";

            add_child_types (child_types, {
                typeof (WixProduct),
                typeof (WixFragment),
            });
        }
    }

    public class WixEnvironment: WixElement {
        static construct {
            name = "Environment";
        }

        public string Action { get; set; }
        public string Name { get; set; }
        public string Part { get; set; }
        public string Permanent { get; set; }
        public string Separator { get; set; }
        public string System { get; set; }
        public string Value { get; set; }

        public override void accept (WixNodeVisitor visitor) throws GLib.Error {
            visitor.visit_environment(this);
        }

    }

    public class WixCopyFile: WixElement {
        static construct {
            name = "CopyFile";
        }

        public string FileId { get; set; }
        public string Delete { get; set; }
        public string DestinationDirectory { get; set; }
        public string DestinationName { get; set; }
        public string SourceDirectory { get; set; }
        public string SourceName { get; set; }
        
        public string Value { get; set; }

        public override void accept (WixNodeVisitor visitor) throws GLib.Error {
            visitor.visit_copy_file(this);
        }

    }

} // Wixl
