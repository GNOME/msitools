namespace Wixl {

    public abstract class WixElementVisitor: Object {
        public abstract void visit_product (WixProduct product) throws GLib.Error;
        public abstract void visit_icon (WixIcon icon) throws GLib.Error;
        public abstract void visit_package (WixPackage package) throws GLib.Error;
        public abstract void visit_property (WixProperty prop) throws GLib.Error;
        public abstract void visit_media (WixMedia media) throws GLib.Error;
    }

    public abstract class WixElement: Object {
        public class string name;
        public string Id { get; set; }
        public List<WixElement> children;

        static construct {
            Value.register_transform_func (typeof (WixElement), typeof (string), (ValueTransform)WixElement.value_to_string);
        }

        public void add_child (WixElement e) {
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
            str += ">\n";

            foreach (var child in children) {
                str += child.to_string () + "\n";
            }

            return str + "</" + name + ">";
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

        public override void load (Xml.Node *node) throws Wixl.Error {
            base.load (node);

            for (var child = node->children; child != null; child = child->next) {
                switch (child->type) {
                case Xml.ElementType.COMMENT_NODE:
                case Xml.ElementType.TEXT_NODE:
                    continue;
                case Xml.ElementType.ELEMENT_NODE:
                    switch (child->name) {
                    }
                    break;
                }
                debug ("unhandled node %s", child->name);
            }
        }

        public override void accept (WixElementVisitor visitor) throws GLib.Error {
            visitor.visit_package (this);
            base.accept (visitor);
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
                    }
                    break;
                }
                debug ("unhandled node %s", child->name);
            }
        }

        public override void accept (WixElementVisitor visitor) throws GLib.Error {
            visitor.visit_product (this);
            base.accept (visitor);
        }
    }

    public class WixMedia: WixElement {
        static construct {
            name = "Media";
        }

        public string Cabinet { get; set; }
        public string EmbedCab { get; set; }
        public string DiskPrompt { get; set; }

        public override void accept (WixElementVisitor visitor) throws GLib.Error {
            visitor.visit_media (this);
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