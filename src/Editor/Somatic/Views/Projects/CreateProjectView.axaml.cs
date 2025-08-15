using Avalonia.Controls;
using Somatic.Model;
using Somatic.ViewModels;

namespace Somatic.Views;

public partial class CreateProjectView : UserControl {
    public CreateProjectView() {
        InitializeComponent();

        searchableListBox.Loaded += (sender, e) => { searchableListBox.FilterFunction = (p, s) => ((TemplateProject)p).Name.ToLower().Contains(s.ToLower());  };
    }

    private void ListBox_SelectionChanged(object sender, SelectionChangedEventArgs e) {
        if (e.AddedItems.Count > 0 && e.AddedItems[0] is not null) {
            TemplateProject project = (e.AddedItems[0] as TemplateProject)!;
            comboEngine.SelectedIndex = (int)project.EngineType;
            comboMode.SelectedIndex = (int)project.EngineMode;
        }
    }

    private void ComboBox_SelectionChanged(object? sender, Avalonia.Controls.SelectionChangedEventArgs e) {
        if (e.AddedItems.Count > 0 && e.AddedItems[0] is not null) {
            if (e.Source is ComboBox comboBox) {
                CreateProjectViewModel project = (DataContext as CreateProjectViewModel)!;

                if (comboBox.Name == "comboEngine") {
                    project.EngineType = (EngineTypes)comboBox.SelectedIndex;
                } else if (comboBox.Name == "comboMode") {
                    project.EngineMode = (EngineModes)comboBox.SelectedIndex;
                }
            }
        }
    }
}