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
                    array = (c as WixElement).add_elements<G> (array);
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
                    var e = (c as WixElement).find_element<G> (Id);
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
                typeof (WixComponentRef),
                typeof (WixComponentGroupRef),
            });
        }
    }

    public class WixFragment: WixElement {
        static construct {
            name = "Fragment";

            add_child_types (child_types, {
                typeof (WixDirectory),
                typeof (WixDirectoryRef),
                typeof (WixComponentGroup),
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
        public string Win64 { get; set; }

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

            add_child_types (child_types, { typeof (WixShortcut) });
        }

        public string DiskId { get; set; }
        public string Source { get; set; }
        public string Name { get; set; }

        public File file;

        public override string path_name () throws GLib.Error {
            return Name;
        }

        public override void accept (WixNodeVisitor visitor) throws GLib.Error {
            base.accept (visitor);
            visitor.visit_file (this);
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

    public class WixFeature: WixElement {
        static construct {
            name = "Feature";

            add_child_types (child_types, {
                typeof (WixComponentRef),
                typeof (WixComponentGroupRef),
                typeof (WixFeature),
            });
        }

        public string Level { get; set; }
        public string Title { get; set; }
        public string Description { get; set; }
        public string Display { get; set; }
        public string ConfigurableDirectory { get; set; }
        public string AllowAdvertise { get; set; }
        public string Absent { get; set; }

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
            return (r.resolve<WixComponent> (this) as WixElement).full_path (r);
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
            return (r.resolve<WixComponentGroup> (this) as WixElement).full_path (r);
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
                    return (c as WixText).Text;
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
                    "FileCost",
                    "FindRelatedProducts",
                    "IsolateComponents",
                    "LaunchConditions",
                    "MigrateFeatureStates",
                    "ResolveSource",
                    "RMCCPSearch",
                    "ScheduleReboot",
                    "Show",
                    "ValidateProductID" })
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
                    "FileCost",
                    "InstallAdminPackage",
                    "InstallFiles",
                    "InstallFinalize",
                    "InstallInitialize",
                    "InstallValidate",
                    "LaunchConditions",
                    "Show" })
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
        public string ExeCommand { get; set; }
        public string Impersonate { get; set; }
        public string Return { get; set; }
        public string BinaryKey { get; set; }
        public string DllEntry { get; set; }
        public string HideTarget { get; set; }
        public string JScriptCall { get; set; }

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
                typeof (WixShortcut),
                typeof (WixProgId),
                typeof (WixRegistryKey),
                typeof (WixServiceControl),
                typeof (WixServiceInstall),
            });
        }

        public string Guid { get; set; }
        public string Win64 { get; set; }
        public WixKeyElement? key;

        public List<WixFeature> in_feature;

        public override void accept (WixNodeVisitor visitor) throws GLib.Error {
            visitor.visit_component (this, VisitState.ENTER);
            base.accept (visitor);
            visitor.visit_component (this, VisitState.LEAVE);
        }

        public override string full_path (WixResolver r) throws GLib.Error {
            if (key == null)
                throw new Wixl.Error.FAILED("a child is needed to generate a component GUID");

            return parent.full_path (r) + "/" + key.path_name ();
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
            return (r.resolve<G> (this) as WixElement).full_path (r);
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
            return (r.resolve<WixDirectory> (this) as WixElement).full_path (r);
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

} // Wixl
