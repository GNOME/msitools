namespace Wixl {

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
        public abstract void visit_component (WixComponent comp) throws GLib.Error;
        public abstract void visit_feature (WixFeature feature, VisitState state) throws GLib.Error;
        public abstract void visit_component_ref (WixComponentRef ref) throws GLib.Error;
        public abstract void visit_remove_folder (WixRemoveFolder rm) throws GLib.Error;
        public abstract void visit_registry_value (WixRegistryValue reg) throws GLib.Error;
        public abstract void visit_file (WixFile reg) throws GLib.Error;
        public abstract void visit_shortcut (WixShortcut shortcut) throws GLib.Error;
        public abstract void visit_create_folder (WixCreateFolder folder) throws GLib.Error;
        public abstract void visit_fragment (WixFragment fragment) throws GLib.Error;
        public abstract void visit_directory_ref (WixDirectoryRef ref) throws GLib.Error;
        public abstract void visit_sequence (WixSequence sequence) throws GLib.Error;
        public abstract void visit_condition (WixCondition condition) throws GLib.Error;
        public abstract void visit_upgrade (WixUpgrade upgrade) throws GLib.Error;
        public abstract void visit_upgrade_version (WixUpgradeVersion version) throws GLib.Error;
        public abstract void visit_action (WixAction action) throws GLib.Error;
        public abstract void visit_text (WixText text) throws GLib.Error;
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

        public virtual void load (Xml.Node *node) throws Wixl.Error {
            if (node->name != name)
                throw new Error.FAILED ("%s: invalid node %s".printf (name, node->name));

            for (var prop = node->properties; prop != null; prop = prop->next) {
                if (prop->type == Xml.ElementType.ATTRIBUTE_NODE)
                    set_property (prop->name, get_attribute_content (prop));
            }

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
                debug ("unhandled child %s node %s", name, child->name);
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

    public class WixFragment: WixElement {
        static construct {
            name = "Fragment";

            add_child_types (child_types, {
                typeof (WixDirectory),
                typeof (WixDirectoryRef),
            });
        }

        public override void accept (WixNodeVisitor visitor) throws GLib.Error {
            base.accept (visitor);
            visitor.visit_fragment (this);
        }
    }

    public class WixProperty: WixElement {
        static construct {
            name = "Property";
        }

        public string Value { get; set; }

        public override void accept (WixNodeVisitor visitor) throws GLib.Error {
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
    }

    public class WixFile: WixKeyElement {
        static construct {
            name = "File";

            add_child_types (child_types, { typeof (WixShortcut) });
        }

        public string DiskId { get; set; }
        public string Source { get; set; }
        public string Name { get; set; }

        public Libmsi.Record record;
        public File file;

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
                typeof (WixFeature),
            });
        }

        public string Level { get; set; }
        public string Title { get; set; }
        public string Description { get; set; }
        public string Display { get; set; }
        public string ConfigurableDirectory { get; set; }

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

        public override void accept (WixNodeVisitor visitor) throws GLib.Error {
            visitor.visit_component_ref (this);
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

    public abstract class WixAction: WixElement {
        public string After { get; set; }
        public string Before { get; set; }
        public string Overridable { get; set; }
        public string Sequence { get; set; }
        public string Suppress { get; set; }

        public override void accept (WixNodeVisitor visitor) throws GLib.Error {
            visitor.visit_action (this);
        }
    }

    public class WixRemoveExistingProducts: WixAction {
        static construct {
            name = "RemoveExistingProducts";
        }
    }

    public class WixLaunchConditions: WixAction {
        static construct {
            name = "LaunchConditions";
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

            add_child_types (child_types, {
                typeof (WixRemoveExistingProducts),
                typeof (WixLaunchConditions),
            });
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

    public class WixProduct: WixElement {
        static construct {
            name = "Product";

            add_child_types (child_types, {
                typeof (WixCondition),
                typeof (WixDirectory),
                typeof (WixFeature),
                typeof (WixIcon),
                typeof (WixInstallExecuteSequence),
                typeof (WixMedia),
                typeof (WixPackage),
                typeof (WixProperty),
                typeof (WixUpgrade),
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

    public class WixComponent: WixElement {
        static construct {
            name = "Component";

            add_child_types (child_types, {
                typeof (WixRemoveFolder),
                typeof (WixRegistryValue),
                typeof (WixFile)
            });
        }

        public string Guid { get; set; }
        public WixKeyElement? key;

        public List<WixFeature> in_feature;

        public override void accept (WixNodeVisitor visitor) throws GLib.Error {
            base.accept (visitor);
            visitor.visit_component (this);
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

        public override void accept (WixNodeVisitor visitor) throws GLib.Error {
            base.accept (visitor);
            visitor.visit_directory_ref (this);
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
