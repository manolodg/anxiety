using Avalonia.Controls;
using Somatic.Model;

namespace Somatic.Views {
    public partial class OpenProjectView : UserControl {
        public OpenProjectView() {
            InitializeComponent();

            projectsListBox.Loaded += (sender, e) => { projectsListBox.FilterFunction = (p, s) => ((ProjectData)p).Name.ToLower().Contains(s.ToLower()); };
        }
    }
}
