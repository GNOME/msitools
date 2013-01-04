namespace Wixl {

    public abstract class WixElementVisitor: Object {
        public abstract void visit_product (WixProduct product) throws GLib.Error;
        public abstract void visit_icon (WixIcon icon) throws GLib.Error;
        public abstract void visit_package (WixPackage package) throws GLib.Error;
        public abstract void visit_property (WixProperty prop) throws GLib.Error;
        public abstract void visit_media (WixMedia media) throws GLib.Error;
        public abstract void visit_directory (WixDirectory dir) throws GLib.Error;
        public abstract void visit_component (WixComponent comp) throws GLib.Error;
        public abstract void visit_feature (WixFeature feature) throws GLib.Error;
        public abstract void visit_component_ref (WixComponentRef ref) throws GLib.Error;
        public abstract void visit_remove_folder (WixRemoveFolder rm) throws GLib.Error;
        public abstract void visit_registry_value (WixRegistryValue reg) throws GLib.Error;
        public abstract void visit_file (WixFile reg) throws GLib.Error;
    }

    public abstract class WixElement: Object {
        public class string name;
        public string Id { get; set; }
        public WixElement parent;
        public List<WixElement> children;

        static construct {
            Value.register_transform_func (typeof (WixElement), typeof (string), (ValueTransform)WixElement.value_to_string);
        }

        public void add_child (WixElement e) {
            e.parent = this;
            children.append (e);
        }

        public virtual void load (Xml.Node *node) throws Wixl.Error {
            if (node->name != name)
                throw new Error.FAILED ("%s: invalid node %s".printf (name, node->name));

            for (var prop = node->properties; prop != null; prop = prop->next) {
                if (prop->type == Xml.ElementType.ATTRIBUTE_NODE)
                    set_property (prop->name, get_attribute_content (prop));
            }
        }

        public string to_string () {
            var type = get_type ();
            var klass = (ObjectClass)type.class_ref ();
            var str = "<" + name;

            var i = 0;
            foreach (var p in klass.list_properties ()) {
                var value = Value (p.value_type);
                get_property (p.name, ref value);
                var valstr = value.holds (typeof (string)) ?
                    (string)value : value.strdup_contents ();
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

        public static void value_to_string (Value src, out Value dest) {
            WixElement e = value_get_element (src);

            dest = e.to_string ();
        }

        public static WixElement? value_get_element (Value value) {
            if (! value.holds (typeof (WixElement)))
                return null;

            return (WixElement)value.get_object ();
        }

        public virtual void accept (WixElementVisitor visitor) throws GLib.Error {
            foreach (var child in children)
                child.accept (visitor);
        }
    }

    public class WixProperty: WixElement {
        static construct {
            name = "Property";
        }

        public string Value { get; set; }

        public override void accept (WixElementVisitor visitor) throws GLib.Error {
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

        public override void accept (WixElementVisitor visitor) throws GLib.Error {
            base.accept (visitor);
            visitor.visit_package (this);
        }
    }

    public class WixIcon: WixElement {
        static construct {
            name = "Icon";
        }

        public string SourceFile { get; set; }

        public override void accept (WixElementVisitor visitor) throws GLib.Error {
            visitor.visit_icon (this);
        }
    }

    public abstract class WixKeyElement: WixElement {
        public string KeyPath { get; set; }
    }

    public class WixFile: WixKeyElement {
        static construct {
            name = "File";
        }

        public string DiskId { get; set; }
        public string Source { get; set; }
        public string Name { get; set; }

        public Libmsi.Record record;

        public override void accept (WixElementVisitor visitor) throws GLib.Error {
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

        public override void accept (WixElementVisitor visitor) throws GLib.Error {
            visitor.visit_registry_value (this);
        }
    }

    public class WixRemoveFolder: WixElement {
        static construct {
            name = "RemoveFolder";
        }

        public string On { get; set; }

        public override void accept (WixElementVisitor visitor) throws GLib.Error {
            visitor.visit_remove_folder (this);
        }
    }

    public class WixFeature: WixElement {
        static construct {
            name = "Feature";
        }

        public string Level { get; set; }

        public override void load (Xml.Node *node) throws Wixl.Error {
            base.load (node);

            for (var child = node->children; child != null; child = child->next) {
                switch (child->type) {
                case Xml.ElementType.COMMENT_NODE:
                case Xml.ElementType.TEXT_NODE:
                    continue;
                case Xml.ElementType.ELEMENT_NODE:
                    switch (child->name) {
                    case "ComponentRef":
                        var ref = new WixComponentRef ();
                        ref.load (child);
                        add_child (@ref);
                        continue;
                    }
                    break;
                }
                debug ("unhandled node %s", child->name);
            }
        }

        public override void accept (WixElementVisitor visitor) throws GLib.Error {
            base.accept (visitor);
            visitor.visit_feature (this);
        }
    }

    public class WixComponentRef: WixElement {
        static construct {
            name = "ComponentRef";
        }

        public override void accept (WixElementVisitor visitor) throws GLib.Error {
            visitor.visit_component_ref (this);
        }
    }

    public class WixProduct: WixElement {
        static construct {
            name = "Product";
        }

        public string Name { get; set; }
        public string UpgradeCode { get; set; }
        public string Language { get; set; }
        public string Codepage { get; set; }
        public string Version { get; set; }
        public string Manufacturer { get; set; }

        public WixProduct () {
        }

        public override void load (Xml.Node *node) throws Wixl.Error {
            base.load (node);

            for (var child = node->children; child != null; child = child->next) {
                switch (child->type) {
                case Xml.ElementType.COMMENT_NODE:
                case Xml.ElementType.TEXT_NODE:
                    continue;
                case Xml.ElementType.ELEMENT_NODE:
                    switch (child->name) {
                    case "Package":
                        var package = new WixPackage ();
                        package.load (child);
                        add_child (package);
                        continue;
                    case "Icon":
                        var icon = new WixIcon ();
                        icon.load (child);
                        add_child (icon);
                        continue;
                    case "Property":
                        var prop = new WixProperty ();
                        prop.load (child);
                        add_child (prop);
                        continue;
                    case "Media":
                        var media = new WixMedia ();
                        media.load (child);
                        add_child (media);
                        continue;
                    case "Directory":
                        var directory = new WixDirectory ();
                        directory.load (child);
                        add_child (directory);
                        continue;
                    case "Feature":
                        var feature = new WixFeature ();
                        feature.load (child);
                        add_child (feature);
                        continue;
                    }
                    break;
                }
                debug ("unhandled node %s", child->name);
            }
        }

        public override void accept (WixElementVisitor visitor) throws GLib.Error {
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

        public override void accept (WixElementVisitor visitor) throws GLib.Error {
            visitor.visit_media (this);
        }
    }

    public class WixComponent: WixElement {
        static construct {
            name = "Component";
        }

        public string Guid { get; set; }
        public WixKeyElement? key;

        public override void load (Xml.Node *node) throws Wixl.Error {
            base.load (node);

            for (var child = node->children; child != null; child = child->next) {
                switch (child->type) {
                case Xml.ElementType.COMMENT_NODE:
                case Xml.ElementType.TEXT_NODE:
                    continue;
                case Xml.ElementType.ELEMENT_NODE:
                    switch (child->name) {
                    case "RemoveFolder":
                        var rm = new WixRemoveFolder ();
                        rm.load (child);
                        add_child (rm);
                        continue;
                    case "RegistryValue":
                        var reg = new WixRegistryValue ();
                        reg.load (child);
                        add_child (reg);
                        continue;
                    case "File":
                        var file = new WixFile ();
                        file.load (child);
                        add_child (file);
                        continue;
                    }
                    break;
                }
                debug ("unhandled node %s", child->name);
            }
        }

        public override void accept (WixElementVisitor visitor) throws GLib.Error {
            base.accept (visitor);
            visitor.visit_component (this);
        }
    }

    public class WixDirectory: WixElement {
        static construct {
            name = "Directory";
        }

        public string Name { get; set; }

        public override void load (Xml.Node *node) throws Wixl.Error {
            base.load (node);

            for (var child = node->children; child != null; child = child->next) {
                switch (child->type) {
                case Xml.ElementType.COMMENT_NODE:
                case Xml.ElementType.TEXT_NODE:
                    continue;
                case Xml.ElementType.ELEMENT_NODE:
                    switch (child->name) {
                    case "Directory":
                        var directory = new WixDirectory ();
                        directory.load (child);
                        add_child (directory);
                        continue;
                    case "Component":
                        var component = new WixComponent ();
                        component.load (child);
                        add_child (component);
                        continue;
                    }
                    break;
                }
                debug ("unhandled node %s", child->name);
            }
        }

        public override void accept (WixElementVisitor visitor) throws GLib.Error {
            base.accept (visitor);
            visitor.visit_directory (this);
        }
    }

    class WixRoot: WixElement {
        static construct {
            name = "Wix";
        }

        public string xmlns { get; set; }

        public WixRoot () {
        }

        public void load_xml (Xml.Doc *doc) throws Wixl.Error {
            var root = doc->children;
            load (root);

            if (root->ns != null)
                xmlns = root->ns->href;

            for (var child = root->children; child != null; child = child->next) {
                switch (child->type) {
                case Xml.ElementType.COMMENT_NODE:
                case Xml.ElementType.TEXT_NODE:
                    continue;
                case Xml.ElementType.ELEMENT_NODE:
                    switch (child->name) {
                    case "Product":
                        var product = new WixProduct ();
                        product.load (child);
                        add_child (product);
                        continue;
                    }
                    break;
                }

                debug ("unhandled node %s", child->name);
            }
        }
    }

} // Wixl